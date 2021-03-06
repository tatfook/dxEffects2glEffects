

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <cstdio>
#include <cstdlib>


#include "hlsl2glsl.h"
#include "hlslCrossCompiler.h"
#include "hlslLinker.h"
// fx parser
#include "driver.h"
#include "dxeffects.h"


struct UniformInfo
{
	std::string name;
	std::string semantic;
	std::string type;
	bool operator == (const UniformInfo& r)
	{
		return r.name == this->name;
	}
	bool operator != (const UniformInfo&r)
	{
		return r.name != this->name;
	}
};

enum class  OutLang
{
	Unkonw,
	GLSL,
	ESSL,
};



std::string ReadFile(const char* fileName)
{
	std::ifstream ifs(fileName);
	if (!ifs)return "";
	std::stringstream buffer;
	buffer << ifs.rdbuf();
	return buffer.str();
}


void printUsage()
{
	std::cout << "Usage:\n dx2gl <-glsl>|<-essl>  <in> <output>"<<std::endl;
	std::cout << "eg:\n dx2gl -glsl c:\\test.hlsl c:\\test.glsl" << std::endl;
	std::cout << "eg:\n dx2gl -elsl c:\\test.hlsl c:\\test.essl" << std::endl;
}

struct IncludeContext
{
	std::string currentFolder;
};

static bool C_DECL IncludeOpenCallback(bool isSystem, const char* fname, const char* parentfname, const char* parent, std::string& output, void* d)
{
	const IncludeContext* data = reinterpret_cast<IncludeContext*>(d);

	std::string pathName = data->currentFolder + "/" + fname;

	output = ReadFile(pathName.c_str());
	return true;
}


static const char* EsTypeToString(EShType type)
{
	switch (type)
	{
	default:
		return "unkonw";
	case EShTypeVoid:
		return "void";
	case EShTypeBool:
		return "bool";
	case EShTypeBVec2:
		return "bvec2";
	case EShTypeBVec3:
		return "bvec3";
	case EShTypeBVec4:
		return "bvec4";
	case EShTypeInt:
		return "int";
	case EShTypeIVec2:
		return "ivec2";
	case EShTypeIVec3:
		return "ivec3";
	case EShTypeIVec4:
		return "vec4";
	case EShTypeFloat:
		return "float";
	case EShTypeVec2:
		return "vec2";
	case EShTypeVec3:
		return "vec3";
	case EShTypeVec4:
		return "vec4";
	case EShTypeMat2:
		return "mat2";
	case EShTypeMat2x3:
		return "mat2x3";
	case EShTypeMat2x4:
		return "mat2x4";
	case EShTypeMat3x2:
		return "mat3x2";
	case EShTypeMat3:
		return "mat3";
	case EShTypeMat3x4:
		return "mat3x4";
	case EShTypeMat4x2:
		return "mat4x2";
	case EShTypeMat4x3:
		return "mat4x3";
	case EShTypeMat4x4:
		return "mat4";
	case EShTypeSampler:
		return "sampler";
	case EShTypeSampler1D:
		return "sampler1D";
	case EShTypeSampler1DShadow:
		return "sampler1DShadow";
	case EShTypeSampler2D:
		return "sampler2D";
	case EShTypeSampler2DShadow:
		return "Sampler2DShadow";
	case EShTypeSampler3D:
		return "Sampler3D";
	case EShTypeSamplerCube:
		return "SamplerCube";
	case EShTypeSamplerRect:
		return "SamplerRect";
	case EShTypeSamplerRectShadow:
		return "SamplerRectShadow";
	case EShTypeSampler2DArray:
		return "Sampler2DArray";
	case EShTypeStruct:
		return "Struct";
	}
}

static void GetUniforms(ShHandle parser,std::vector<UniformInfo>& uniforms)
{
	int count = Hlsl2Glsl_GetUniformCount(parser);
	if (count > 0)
	{
		const ShUniformInfo* uni = Hlsl2Glsl_GetUniformInfo(parser);

		for (int i = 0; i < count; ++i)
		{
			UniformInfo info;
			info.name = uni[i].name;
			if (uni[i].semantic != nullptr) {
				info.semantic = uni[i].semantic;
			}
			info.type = EsTypeToString(uni[i].type);

			if (std::find(uniforms.begin(), uniforms.end(), info)==uniforms.end()) {
				uniforms.push_back(info);
			}		
		}
	}
}

static std::string GetCompiledShaderText(ShHandle parser)
{
	std::string txt = Hlsl2Glsl_GetShader(parser);
	return txt;
}




bool hlsl2glsl(const std::string inputPath,const std::string& inCode,const std::string& enterpoint,EShLanguage toLang, ETargetVersion toVersion,std::string& outCode, std::vector<UniformInfo>& uniforms)
{

	

	// Include Handle
	IncludeContext includeCtx;
	includeCtx.currentFolder = inputPath.substr(0, inputPath.rfind('/'));
	Hlsl2Glsl_ParseCallbacks includeCB;
	includeCB.includeOpenCallback = IncludeOpenCallback;
	includeCB.includeCloseCallback = NULL;
	includeCB.data = &includeCtx;

	ShHandle parser = Hlsl2Glsl_ConstructCompiler(toLang);

	auto linker = parser->GetLinker();
	linker->setUserAttribName(EAttribSemantic::EAttrSemPosition, "a_position");
	linker->setUserAttribName(EAttribSemantic::EAttrSemNormal, "a_normal");
	linker->setUserAttribName(EAttribSemantic::EAttrSemColor0, "a_color");
	linker->setUserAttribName(EAttribSemantic::EAttrSemColor1, "a_color2");
	linker->setUserAttribName(EAttribSemantic::EAttrSemTex0,"a_texCoord");
	linker->setUserAttribName(EAttribSemantic::EAttrSemTex1, "a_texCoord1");
	linker->setUserAttribName(EAttribSemantic::EAttrSemTex2, "a_texCoord2");
	linker->setUserAttribName(EAttribSemantic::EAttrSemTex3, "a_texCoord3");
	linker->setUserAttribName(EAttribSemantic::EAttrSemBlendWeight, "a_blendWeight");
	linker->setUserAttribName(EAttribSemantic::EAttrSemBlendIndices, "a_blendIndex");

	const char* sourceStr = inCode.c_str();
	const char* infoLog = nullptr;
	int opt = ETranslateOpBGRAVertexColor;
	int parseOk = Hlsl2Glsl_Parse(parser, sourceStr, toVersion, &includeCB, opt);
	if (!parseOk) {
		infoLog = Hlsl2Glsl_GetInfoLog(parser);
		std::cerr << infoLog << std::endl;
		Hlsl2Glsl_DestructCompiler(parser);
		return false;

	}
	int translateOk = Hlsl2Glsl_Translate(parser,enterpoint.c_str(), toVersion, opt);
	if (!translateOk) {
		infoLog = Hlsl2Glsl_GetInfoLog(parser);
		std::cerr << infoLog << std::endl;
		Hlsl2Glsl_DestructCompiler(parser);
		return false;
	}
	outCode = GetCompiledShaderText(parser);

	// remove #line and #version
	std::stringstream outss;
	std::istringstream iss(outCode);
	char buf[1024] = {0};
	while(iss.getline(buf,sizeof(buf))){
		std::string line(buf);
		if(line.find("#line ") != std::string::npos || 
		line.find("#version ") != std::string::npos)
		{
			//skip
			continue;
		}
		outss<<line<<std::endl;
	}
	outCode = outss.str();

	GetUniforms(parser, uniforms);
	Hlsl2Glsl_DestructCompiler(parser);
	return true;
}


std::string get_file_name(std::string path)
{
	size_t pos = path.find_last_of('/');
	if (pos == std::string::npos) {
		pos = path.find_first_of('\\');
	}
	if (pos == 0)return path;

	return path.substr(pos + 1);
}


bool translate_hlfx_to_glfx(OutLang outLang,const std::string& inPath,const std::string outPath)
{
	

	DxEffectsTree fxTree;
	DxEffectsParser::Driver driver(fxTree);
	if (!driver.parse_file(inPath)) {
		return false;
	}

	auto code_block = fxTree.getCodeBlock();
	
	
	Hlsl2Glsl_Initialize();


	std::vector<UniformInfo> uniforms;
	std::stringstream uniformsOut;
	std::stringstream glslCodeBlockOut;
	std::stringstream esslCodeBlockOut;

	auto techniques = fxTree.getTechiques();

	std::stringstream techout;
	for (auto technique : techniques)
	{

		techout << "technique " << technique->getName() << std::endl;
		techout << "{" << std::endl;

		auto passes = technique->getPasses();	
		std::cout << "Parse Technique:" + technique->getName() << " Total Pass:" <<passes.size()<< std::endl;
		for (auto pass:passes)
		{
			std::stringstream passout;
			techout << "\tpass " << pass->getName() << std::endl;
			techout << "\t{" << std::endl;

			auto states = pass->getStateAssignments();
			std::cout << "Parse Pass:" + pass->getName() << " Total StateAssignment:" << states.size() << std::endl;
			for (auto state:states)
			{
				std::stringstream stateOut;
				stateOut << "\t\t" << state->getName();
				if (state->getNameIndex() != -1) {
					stateOut << "[" << state->getNameIndex() << "]";
				}
				stateOut << " = ";
				auto stateValue = state->getValue();
				auto stateValueType = stateValue->getValueType();
				switch (stateValueType)
				{
				case StateValueType::COMPILE:
				{
					auto value = static_cast<const StateCompileValue*>(stateValue);
					auto target = value->getTarget();
					auto enterpoint = value->getEntryPoint();
					EShLanguage toLang = EShLangCount;
					if (target.find("vs_") != std::string::npos) {
						toLang = EShLangVertex;
						stateOut << "compile vs " << enterpoint << "()";
					}
					else if (target.find("ps_") != std::string::npos) {
						toLang = EShLangFragment;
						stateOut << "compile ps " << enterpoint << "()";
					}
					else
					{
						std::cerr << "unkonw compile target " << target << std::endl;
						Hlsl2Glsl_Shutdown();
						return false;
					}
	
					std::string glslCode;
					std::string esslCode;
					bool ret = false;
					ret = hlsl2glsl(inPath, code_block, enterpoint, toLang, ETargetVersion::ETargetGLSL_110, glslCode,uniforms);
					if (!ret) {
						Hlsl2Glsl_Shutdown();
						return false;
					}
					ret = hlsl2glsl(inPath, code_block, enterpoint, toLang, ETargetVersion::ETargetGLSL_ES_100, esslCode,uniforms);
					if (!ret) {
						Hlsl2Glsl_Shutdown();
						return false;
					}



					glslCodeBlockOut << "#CODEBLOCK_BEGIN" << std::endl;
					glslCodeBlockOut << glslCode << std::endl;
					glslCodeBlockOut << "#CODEBLOCK_END " << enterpoint << "\n\n\n";

					esslCodeBlockOut << "#CODEBLOCK_BEGIN" << std::endl;
					esslCodeBlockOut << esslCode << std::endl;
					esslCodeBlockOut << "#CODEBLOCK_END " << enterpoint << "\n\n\n";
					
				}
				break;
				case StateValueType::BOOLEAN:
					stateOut << stateValue->toString();
					break;
				case StateValueType::STRING:
					stateOut << stateValue->toString();
					break;
				case StateValueType::INTEGER:
					stateOut << stateValue->toString();
					break;
				case StateValueType::FLOAT:
					stateOut << stateValue->toString();
					break;
				case StateValueType::FLOAT2:
					stateOut << stateValue->toString();
					break;
				case StateValueType::FLOAT3:
					stateOut << stateValue->toString();
					break;
				case StateValueType::FLOAT4:
					stateOut << stateValue->toString();
					break;
				case StateValueType::D3DCOLOR:
					stateOut << stateValue->toString();
					break;
				case StateValueType::UNKNOWN:
					std::cerr << "WARNING: Unkonw State type " << state->getName();
					break;
				default:
					std::cerr << "WARNING: Unkonw State type " << state->getName();
					break;
				}
				stateOut << ";";
				passout << stateOut.str() << std::endl;
			}

			passout << "\t}";

			techout << passout.str() << std::endl;
		}

		techout << "}" << std::endl;
	}

	// output uniforms

	uniformsOut << "Unifroms\n{\n";
	for(auto uniform : uniforms)
	{
		uniformsOut <<"\t"<< uniform.type << " " << uniform.name;
		if (uniform.semantic.length() > 0) {
			uniformsOut << ":" << uniform.semantic;
		}
		uniformsOut << ";\n";
	}
	uniformsOut << "\n}\n";

	
	// write to file



	std::ofstream of;
	of.open(outPath);
	if (!of.is_open()) {
		std::cerr << "Can't open to write " + outPath << std::endl;
		Hlsl2Glsl_Shutdown();
		return false;
		
	}
	if (outLang == OutLang::GLSL) {
		of << uniformsOut.str() << glslCodeBlockOut.str() << std::endl << techout.str();
	}
	else if (outLang == OutLang::ESSL)
	{
		of << uniformsOut.str() << esslCodeBlockOut.str() << std::endl << techout.str();
	}else
	{
		std::cerr << "Unkow out lang "<< std::endl;
		of.close();
		Hlsl2Glsl_Shutdown();
		return false;
	}
	of.close();
	Hlsl2Glsl_Shutdown();
	return true;
}




int main(int argc,char** argv)
{

	OutLang lang = OutLang::Unkonw;
	if (argc < 4)
	{
		printUsage();
		return 0;
	}
	else {
		std::string opt_lang = argv[1];
		if (opt_lang == "-glsl" || opt_lang == "-g")
		{
			lang = OutLang::GLSL;
		}
		else if (opt_lang == "-essl" || opt_lang == "-e")
		{
			lang = OutLang::ESSL;
		}
		else {
			lang = OutLang::Unkonw;
		}

		if (lang == OutLang::Unkonw)
		{
			std::cerr << "bad option" + opt_lang << std::endl;
			printUsage();
			return 0;
		}
	}


	
	std::string inputPath = argv[2];
	std::string outputDir = argv[3];

	if (translate_hlfx_to_glfx(lang,inputPath, outputDir)) {
		std::cout << "\bSucessed!\n";
	}
	else {
		std::cout << "\bFailed!\n";
	}
	return 0;
}

