/*
 * fbx2anm: converts .FBX animations to HPL2 .anm files (MSH format v7) without the Autodesk FBX SDK.
 *
 * Reads the FBX with Assimp and mirrors what HPL2's cMeshLoaderFBX::LoadAnimation did with FBX SDK 2012
 * (HPL2/core/sources/impl/MeshLoaderFBX.cpp), then writes the file with the engine's own
 * cMeshLoaderMSH::SaveAnimation.
 *
 *  - Skeleton  = FBX Model nodes with a skeleton attribute (LimbNode/Root/Limb), nested like the scene.
 *  - Rest pose = skin cluster TransformLink (x geometric transform of the link node), split into T, R(xyz euler), S
 *                and rebuilt as T*R*S; bones without a cluster keep identity. MakeFinalBonesRec turns these
 *                globals into locals.
 *  - Take      = first animation stack with a length > 0; length = LocalStop in whole milliseconds.
 *  - Tracks    = one per skeleton node, depth first in scene order, flags Translate|Rotate.
 *  - Keys      = union of the node's T/R/S curve key times, truncated to whole milliseconds; the local transform
 *                is evaluated at that time, then trans -= rest local translation and rot = inv(rest rot) * rot.
 *
 * Usage: fbx2anm <in.fbx> <out.anm> [-v]
 *        fbx2anm --list <listfile>        (lines: in.fbx<TAB>out.anm)
 *
 * This file is part of machineForPigsVR and is licensed under the GPLv3 like the HPL2 engine it links.
 */

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/config.h>

// Assimp's FBX DOM (internal headers) for skeleton attributes, clusters and stack times.
#include "FBXTokenizer.h"
#include "FBXParser.h"
#include "FBXDocument.h"
#include "FBXImportSettings.h"
#include "FBXProperties.h"
#include "Common/StackAllocator.h"

#include "graphics/Animation.h"
#include "graphics/AnimationTrack.h"
#include "impl/MeshLoaderMSH.h"
#include "math/Math.h"
#include "system/String.h"

#include <cmath>
#include <cstdio>
#include <fstream>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <vector>

using namespace hpl;

static bool gVerbose = false;

// The engine library's WinMain wrapper references hplMain; this tool has its own console main().
int hplMain(const tString &asCommandLine) { return 0; }

// FBX KTime units per second.
static const double kFbxTicksPerSecond = 46186158000.0;

//------------------------------------------------------------------------------------------------
// Math helpers between Assimp (column vectors, like HPL2) and HPL2 types

static cMatrixf ToHpl(const aiMatrix4x4 &m)
{
	return cMatrixf(m.a1, m.a2, m.a3, m.a4,
					m.b1, m.b2, m.b3, m.b4,
					m.c1, m.c2, m.c3, m.c4,
					m.d1, m.d2, m.d3, m.d4);
}

static cMatrixf RotationMatrix(const aiQuaternion &q)
{
	aiMatrix3x3 r = q.GetMatrix();
	return cMatrixf(r.a1, r.a2, r.a3, 0,
					r.b1, r.b2, r.b3, 0,
					r.c1, r.c2, r.c3, 0,
					0, 0, 0, 1);
}

static aiMatrix4x4 GeometricTransform(const Assimp::FBX::Model &model)
{
	const Assimp::FBX::PropertyTable &props = model.Props();
	bool ok;
	aiVector3D t = Assimp::FBX::PropertyGet<aiVector3D>(props, "GeometricTranslation", ok); if (!ok) t = aiVector3D();
	aiVector3D r = Assimp::FBX::PropertyGet<aiVector3D>(props, "GeometricRotation", ok); if (!ok) r = aiVector3D();
	aiVector3D s = Assimp::FBX::PropertyGet<aiVector3D>(props, "GeometricScaling", ok); if (!ok) s = aiVector3D(1, 1, 1);

	const float d2r = (float)(AI_MATH_PI / 180.0);
	aiMatrix4x4 mt, mx, my, mz, ms;
	aiMatrix4x4::Translation(t, mt);
	aiMatrix4x4::RotationX(r.x * d2r, mx);
	aiMatrix4x4::RotationY(r.y * d2r, my);
	aiMatrix4x4::RotationZ(r.z * d2r, mz);
	aiMatrix4x4::Scaling(s, ms);
	return mt * mz * my * mx * ms;
}

//------------------------------------------------------------------------------------------------
// Skeleton built like cMeshLoaderFBX (LoadSkeletonRec + cluster links + MakeFinalBonesRec)

struct Bone
{
	std::string name;
	int parent = -1;
	bool linked = false;
	cMatrixf global = cMatrixf::Identity;          // rest global (linked) as set from the cluster
	cMatrixf globalUnscaled = cMatrixf::Identity;
	cMatrixf local = cMatrixf::Identity;           // after MakeFinalBonesRec
	cMatrixf localUnscaled = cMatrixf::Identity;
	cMatrixf world = cMatrixf::Identity;           // world from the final local chain
	cMatrixf worldUnscaled = cMatrixf::Identity;
};

// LazyObject::Get throws for elements with fewer than 3 tokens (e.g. GlobalSettings); skip those.
static const Assimp::FBX::Object *SafeGet(Assimp::FBX::LazyObject *lazy)
{
	if (lazy->GetElement().Tokens().size() < 3) return nullptr;
	try { return lazy->Get(); } catch (...) { return nullptr; }
}

struct Converter
{
	const aiScene *scene = nullptr;
	const Assimp::FBX::Document *doc = nullptr;

	std::set<std::string> skeletonNames;
	std::vector<Bone> bones;
	std::map<std::string, int> boneIndex;
	std::vector<const aiNode *> trackNodes;   // skeleton nodes, depth first in scene order

	void CollectSkeletonNames()
	{
		for (const auto &kv : doc->Objects())
		{
			const Assimp::FBX::Object *obj = SafeGet(kv.second);
			const Assimp::FBX::Model *model = dynamic_cast<const Assimp::FBX::Model *>(obj);
			if (!model) continue;
			// FBX SDK's GetSkeleton() is true for skeleton attributes (LimbNode, Root, Limb). Assimp only builds
			// LimbNode attributes, so also look at the Model's class tag.
			bool isSkeleton = false;
			const Assimp::FBX::TokenList &tok = kv.second->GetElement().Tokens();
			if (tok.size() >= 3)
			{
				std::string cls = Assimp::FBX::ParseTokenAsString(*tok[2]);
				isSkeleton = cls == "LimbNode" || cls == "Root" || cls == "Limb";
			}
			for (const Assimp::FBX::NodeAttribute *attr : model->GetAttributes())
				if (dynamic_cast<const Assimp::FBX::LimbNode *>(attr)) isSkeleton = true;
			if (isSkeleton)
			{
				std::string name = model->Name();
				if (name.compare(0, 7, "Model::") == 0) name = name.substr(7);
				skeletonNames.insert(name);
			}
		}
	}

	void BuildSkeletonRec(const aiNode *node, int parentBone)
	{
		std::string name = node->mName.C_Str();
		if (skeletonNames.count(name))
		{
			Bone b;
			b.name = name;
			b.parent = parentBone;
			bones.push_back(b);
			parentBone = (int)bones.size() - 1;
			boneIndex[name] = parentBone;
			trackNodes.push_back(node);
		}
		for (unsigned i = 0; i < node->mNumChildren; ++i) BuildSkeletonRec(node->mChildren[i], parentBone);
	}

	const Assimp::FBX::Model *FindModel(const std::string &name)
	{
		for (const auto &kv : doc->Objects())
		{
			const Assimp::FBX::Model *model = dynamic_cast<const Assimp::FBX::Model *>(SafeGet(kv.second));
			if (!model) continue;
			std::string n = model->Name();
			if (n.compare(0, 7, "Model::") == 0) n = n.substr(7);
			if (n == name) return model;
		}
		return nullptr;
	}

	void LinkClusters()
	{
		// Same order as the FBX SDK walk: every skin cluster sets the global rest pose of its bone.
		for (const auto &kv : doc->Objects())
		{
			const Assimp::FBX::Cluster *cl = dynamic_cast<const Assimp::FBX::Cluster *>(SafeGet(kv.second));
			if (!cl || !cl->TargetNode()) continue;
			std::string name = cl->TargetNode()->Name();
			if (name.compare(0, 7, "Model::") == 0) name = name.substr(7);
			auto it = boneIndex.find(name);
			if (it == boneIndex.end()) continue;

			aiMatrix4x4 link = cl->TransformLink() * GeometricTransform(*cl->TargetNode());
			aiVector3D s, t;
			aiQuaternion r;
			link.Decompose(s, r, t);

			cMatrixf mtxR = RotationMatrix(r);
			cMatrixf mtxBone = cMath::MatrixMul(cMath::MatrixTranslate(cVector3f(t.x, t.y, t.z)),
												cMath::MatrixMul(mtxR, cMath::MatrixScale(cVector3f(s.x, s.y, s.z))));
			Bone &b = bones[it->second];
			b.global = mtxBone;
			b.globalUnscaled = mtxR;
			b.linked = true;
		}
	}

	void MakeFinalBones()
	{
		// bones are stored parent-before-child, so a single pass works like the recursion.
		std::vector<cMatrixf> parentGlobalForChildren(bones.size()), parentGlobalUnscaledForChildren(bones.size());
		for (size_t i = 0; i < bones.size(); ++i)
		{
			Bone &b = bones[i];
			cMatrixf pg = b.parent < 0 ? cMatrixf::Identity : parentGlobalForChildren[b.parent];
			cMatrixf pgu = b.parent < 0 ? cMatrixf::Identity : parentGlobalUnscaledForChildren[b.parent];
			cMatrixf parentWorld = b.parent < 0 ? cMatrixf::Identity : bones[b.parent].world;
			cMatrixf parentWorldU = b.parent < 0 ? cMatrixf::Identity : bones[b.parent].worldUnscaled;

			if (b.linked)
			{
				b.local = cMath::MatrixMul(cMath::MatrixInverse(pg), b.global);
				b.localUnscaled = cMath::MatrixMul(cMath::MatrixInverse(pgu), b.globalUnscaled);
				b.world = cMath::MatrixMul(parentWorld, b.local);
				b.worldUnscaled = cMath::MatrixMul(parentWorldU, b.localUnscaled);
				parentGlobalForChildren[i] = b.global;
				parentGlobalUnscaledForChildren[i] = b.globalUnscaled;
			}
			else
			{
				b.world = parentWorld;            // local stays identity
				b.worldUnscaled = parentWorldU;
				parentGlobalForChildren[i] = b.world;
				parentGlobalUnscaledForChildren[i] = b.worldUnscaled;
			}
		}
	}

	//--------------------------------------------------------------------------------------------
	// Animation sampling

	static aiVector3D LerpVec(const aiVectorKey *keys, unsigned n, double t, aiVector3D def)
	{
		if (n == 0) return def;
		if (t <= keys[0].mTime) return keys[0].mValue;
		for (unsigned i = 1; i < n; ++i)
		{
			if (t <= keys[i].mTime)
			{
				double f = (t - keys[i - 1].mTime) / (keys[i].mTime - keys[i - 1].mTime);
				return keys[i - 1].mValue + (keys[i].mValue - keys[i - 1].mValue) * (ai_real)f;
			}
		}
		return keys[n - 1].mValue;
	}

	static aiQuaternion SlerpQuat(const aiQuatKey *keys, unsigned n, double t, aiQuaternion def)
	{
		if (n == 0) return def;
		if (t <= keys[0].mTime) return keys[0].mValue;
		for (unsigned i = 1; i < n; ++i)
		{
			if (t <= keys[i].mTime)
			{
				double f = (t - keys[i - 1].mTime) / (keys[i].mTime - keys[i - 1].mTime);
				aiQuaternion out;
				aiQuaternion::Interpolate(out, keys[i - 1].mValue, keys[i].mValue, (ai_real)f);
				return out.Normalize();
			}
		}
		return keys[n - 1].mValue;
	}

	const aiNodeAnim *FindChannel(const aiAnimation *anim, const std::string &name)
	{
		for (unsigned i = 0; i < anim->mNumChannels; ++i)
			if (name == anim->mChannels[i]->mNodeName.C_Str()) return anim->mChannels[i];
		return nullptr;
	}

	bool Convert(const std::string &inFile, const std::string &outFile)
	{
		// ---- Assimp scene (pivots and pre/post rotations folded into the node keys)
		Assimp::Importer importer;
		importer.SetPropertyBool(AI_CONFIG_IMPORT_FBX_PRESERVE_PIVOTS, false);
		importer.SetPropertyBool(AI_CONFIG_IMPORT_FBX_OPTIMIZE_EMPTY_ANIMATION_CURVES, false);
		importer.SetPropertyBool(AI_CONFIG_IMPORT_REMOVE_EMPTY_BONES, false);
		scene = importer.ReadFile(inFile, 0);
		if (!scene) { fprintf(stderr, "%s: assimp: %s\n", inFile.c_str(), importer.GetErrorString()); return false; }

		// ---- FBX DOM of the same file
		std::ifstream f(inFile, std::ios::binary);
		std::vector<char> data((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
		data.push_back(0);
		Assimp::FBX::TokenList tokens;
		Assimp::StackAllocator alloc;
		bool binary = strncmp(data.data(), "Kaydara FBX Binary", 18) == 0;
		if (binary) Assimp::FBX::TokenizeBinary(tokens, data.data(), data.size(), alloc);
		else Assimp::FBX::Tokenize(tokens, data.data(), alloc);
		Assimp::FBX::Parser parser(tokens, alloc, binary);
		Assimp::FBX::ImportSettings settings;
		settings.preservePivots = false;
		Assimp::FBX::Document document(parser, settings);
		doc = &document;

		// ---- Skeleton
		CollectSkeletonNames();
		BuildSkeletonRec(scene->mRootNode, -1);
		LinkClusters();
		MakeFinalBones();

		// ---- Take: first stack with a length > 0 (times in whole ms like KTime::GetMilliSeconds)
		const Assimp::FBX::AnimationStack *stack = nullptr;
		long long startMs = 0, stopMs = 0;
		for (const Assimp::FBX::AnimationStack *s : document.AnimationStacks())
		{
			long long a = (long long)std::floor(s->LocalStart() / (kFbxTicksPerSecond / 1000.0));
			long long b = (long long)std::floor(s->LocalStop() / (kFbxTicksPerSecond / 1000.0));
			if (b - a > 0) { stack = s; startMs = a; stopMs = b; break; }
		}
		std::string takeName;
		if (stack)
		{
			takeName = stack->Name();
			if (takeName.compare(0, 11, "AnimStack::") == 0) takeName = takeName.substr(11);
		}
		else
		{
			// Older files keep the take time span (what FBX SDK reports as TakeInfo::mLocalTimeSpan) in the
			// "Takes" section: Take: "name" { LocalTime: start,stop }.
			const Assimp::FBX::Element *takes = parser.GetRootScope().FindElementCaseInsensitive("Takes");
			if (takes && takes->Compound())
			{
				auto range = takes->Compound()->GetCollection("Take");
				for (auto it = range.first; it != range.second; ++it)
				{
					const Assimp::FBX::Element *take = it->second;
					if (!take->Compound() || take->Tokens().empty()) continue;
					const Assimp::FBX::Element *lt = take->Compound()->FindElementCaseInsensitive("LocalTime");
					if (!lt || lt->Tokens().size() < 2) continue;
					long long a = (long long)std::floor(Assimp::FBX::ParseTokenAsInt64(*lt->Tokens()[0]) / (kFbxTicksPerSecond / 1000.0));
					long long b = (long long)std::floor(Assimp::FBX::ParseTokenAsInt64(*lt->Tokens()[1]) / (kFbxTicksPerSecond / 1000.0));
					if (b - a > 0)
					{
						takeName = Assimp::FBX::ParseTokenAsString(*take->Tokens()[0]);
						startMs = a; stopMs = b;
						break;
					}
				}
			}
			if (takeName.empty()) { fprintf(stderr, "%s: no animation take with a length\n", inFile.c_str()); return false; }
		}

		const aiAnimation *anim = nullptr;
		for (unsigned i = 0; i < scene->mNumAnimations; ++i)
		{
			std::string n = scene->mAnimations[i]->mName.C_Str();
			if (n == takeName || (stack && n == stack->Name()) || n.find(takeName) != std::string::npos) { anim = scene->mAnimations[i]; break; }
		}
		if (!anim && scene->mNumAnimations > 0) anim = scene->mAnimations[0];
		double ticksPerSecond = (anim && anim->mTicksPerSecond > 0) ? anim->mTicksPerSecond : 1.0;

		tWString wOut = cString::To16Char(outFile);
		tWString wIn = cString::To16Char(inFile);
		cAnimation *pAnimation = new cAnimation(takeName, wIn, cString::GetFileName(inFile));
		pAnimation->SetLength((float)stopMs / 1000.0f);

		for (size_t ti = 0; ti < trackNodes.size(); ++ti)
		{
			const aiNode *node = trackNodes[ti];
			std::string name = node->mName.C_Str();
			const Bone &bone = bones[boneIndex[name]];
			const aiNodeAnim *ch = anim ? FindChannel(anim, name) : nullptr;

			// Key times: union of T/R/S key times, as whole milliseconds.
			std::set<float> times;
			if (ch)
			{
				auto addTime = [&](double ticks) {
					double sec = ticks / ticksPerSecond;
					long long ms = (long long)std::floor(sec * 1000.0 + 1e-6);
					times.insert((float)ms / 1000.0f);
				};
				for (unsigned k = 0; k < ch->mNumPositionKeys; ++k) addTime(ch->mPositionKeys[k].mTime);
				for (unsigned k = 0; k < ch->mNumRotationKeys; ++k) addTime(ch->mRotationKeys[k].mTime);
				for (unsigned k = 0; k < ch->mNumScalingKeys; ++k) addTime(ch->mScalingKeys[k].mTime);
			}

			cAnimationTrack *pTrack = pAnimation->CreateTrack(name, eAnimTransformFlag_Translate | eAnimTransformFlag_Rotate);

			cQuaternion qInvBoneRot;
			qInvBoneRot.FromRotationMatrix(bone.localUnscaled.GetRotation());
			qInvBoneRot.v = qInvBoneRot.v * -1;
			qInvBoneRot.Normalize();
			cVector3f vRestTrans = bone.local.GetTranslation();

			aiVector3D defT, defS;
			aiQuaternion defR;
			node->mTransformation.Decompose(defS, defR, defT);
			aiMatrix4x4 geom;
			if (const Assimp::FBX::Model *model = FindModel(name)) geom = GeometricTransform(*model);

			for (float t : times)
			{
				double ticks = (double)t * ticksPerSecond;
				aiVector3D T = LerpVec(ch->mPositionKeys, ch->mNumPositionKeys, ticks, defT);
				aiQuaternion R = SlerpQuat(ch->mRotationKeys, ch->mNumRotationKeys, ticks, defR);
				aiVector3D S = LerpVec(ch->mScalingKeys, ch->mNumScalingKeys, ticks, defS);

				aiMatrix4x4 mt, ms;
				aiMatrix4x4::Translation(T, mt);
				aiMatrix4x4::Scaling(S, ms);
				aiMatrix4x4 local = mt * aiMatrix4x4(R.GetMatrix()) * ms * geom;
				aiVector3D lt, ls;
				aiQuaternion lr;
				local.Decompose(ls, lr, lt);

				cKeyFrame *pKey = pTrack->CreateKeyFrame(t);
				pKey->trans = cVector3f(lt.x, lt.y, lt.z) - vRestTrans;
				cQuaternion qFinal(lr.w, lr.x, lr.y, lr.z);
				qFinal.Normalize();
				pKey->rotation = cMath::QuaternionMul(qInvBoneRot, qFinal);
				pKey->rotation.Normalize();
			}

			if (gVerbose)
				printf("  track %-32s keys=%d linked=%d\n", name.c_str(), pTrack->GetKeyFrameNum(), bone.linked ? 1 : 0);
		}

		cMeshLoaderMSH saver(NULL);
		bool ok = saver.SaveAnimation(pAnimation, wOut);
		printf("%s -> %s: take '%s' %.3fs, %d tracks %s\n", inFile.c_str(), outFile.c_str(), takeName.c_str(),
			   pAnimation->GetLength(), pAnimation->GetTrackNum(), ok ? "OK" : "FAILED");
		delete pAnimation;
		return ok;
	}
};

int main(int argc, char **argv)
{
	if (argc < 3)
	{
		fprintf(stderr, "usage: fbx2anm <in.fbx> <out.anm> [-v] | fbx2anm --list <listfile>\n");
		return 2;
	}
	for (int i = 1; i < argc; ++i) if (std::string(argv[i]) == "-v") gVerbose = true;

	std::vector<std::pair<std::string, std::string>> jobs;
	if (std::string(argv[1]) == "--list")
	{
		std::ifstream lst(argv[2]);
		std::string line;
		while (std::getline(lst, line))
		{
			if (!line.empty() && line.back() == '\r') line.pop_back();
			size_t tab = line.find('\t');
			if (tab != std::string::npos) jobs.emplace_back(line.substr(0, tab), line.substr(tab + 1));
		}
	}
	else jobs.emplace_back(argv[1], argv[2]);

	int failed = 0;
	for (auto &job : jobs)
	{
		try
		{
			Converter c;
			if (!c.Convert(job.first, job.second)) ++failed;
		}
		catch (const std::exception &e)
		{
			fprintf(stderr, "%s: exception: %s\n", job.first.c_str(), e.what());
			++failed;
		}
	}
	printf("done: %d converted, %d failed\n", (int)jobs.size() - failed, failed);
	return failed ? 1 : 0;
}
