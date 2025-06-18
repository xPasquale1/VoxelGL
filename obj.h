#pragma once

#include "windowgl.h"

//TODO vllt weg? Oder zumindest passender benennen wie TriangleVertexPositions oder so
struct Triangle{
	fvec3 points[3];
};

//Speichert ein Material aus einer .mtl file
struct Material{
	std::string name;
	DWORD baseColor = RGBA(0, 0, 0);
	Image textures[3];		//TODO sollte dynamisch sein
	BYTE textureCount = 0;
};

void destroyMaterial(Material& material)noexcept{
	for(BYTE i=0; i < material.textureCount; ++i){
		destroyImage(material.textures[i]);
	}
}

//Speichert ein Modell + optionales Material
//TODO MUSS eine create Funktion bekommen, da destroy davon ausgeht, dass gewisste Daten auf dem Heap liegen
//TODO könnte dann noch sowas wie eine AABB bekommen oder so, um das clipping zu beschleunigen
struct TriangleModel{
	Triangle* triangles = nullptr;
	DWORD triangleCount = 0;
	DWORD triangleCapacity = 0;
	Material* material = nullptr;
	float* attributesBuffer = nullptr;
	BYTE attributesCount;
};

void destroyTriangleModel(TriangleModel& model)noexcept{
	dealloc(model.triangles);
	dealloc(model.attributesBuffer);
	// dealloc(model.material);	//TODO hm is doof
}

ErrCode increaseTriangleCapacity(TriangleModel& model, DWORD additionalCapacity)noexcept{
	Triangle* newArray = alloc<Triangle>(model.triangleCapacity+additionalCapacity, "Trianglebuffer");
	float* newAttributeArray = alloc<float>((model.triangleCapacity+additionalCapacity)*model.attributesCount*3, "Triangleattributebuffer");
	if(newArray == nullptr || newAttributeArray == nullptr) return BAD_ALLOC;
	for(DWORD i=0; i < model.triangleCount; ++i){
		newArray[i] = model.triangles[i];
	}
	for(DWORD i=0; i < model.triangleCount*model.attributesCount*3; ++i){
		newAttributeArray[i] = model.attributesBuffer[i];
	}
	Triangle* oldArray = model.triangles;
	model.triangles = newArray;
	dealloc(oldArray);
	float* oldAttributeArray = model.attributesBuffer;
	model.attributesBuffer = newAttributeArray;
	dealloc(oldAttributeArray);
	model.triangleCapacity += additionalCapacity;
	return SUCCESS;
}

ErrCode splitString(const std::string& string, DWORD& value0, DWORD& value1, DWORD& value2)noexcept{
	std::string buffer[3];
	BYTE idx = 0;
	for(size_t i=0; i < string.size(); ++i){
		if(string[i] == '/'){
			idx++;
			if(idx == 3) return ERR_MODEL_BAD_FORMAT;
			continue;
		};
		buffer[idx] += string[i];
	}
	if(idx < 2) return ERR_MODEL_BAD_FORMAT;
	//TODO Exeptions abfangen
	value0 = (buffer[0].size() < 1) ? 0 : std::stoul(buffer[0].c_str())-1;
	value1 = (buffer[1].size() < 1) ? 0 : std::stoul(buffer[1].c_str())-1;
	value2 = (buffer[2].size() < 1) ? 0 : std::stoul(buffer[2].c_str())-1;
	return SUCCESS;
}

//Gibt das Keyword der obj/mtl Zeile als Zahlenwert zurück, für z.b. die Verwendung in einem switch case
//TODO Hashfunktion ist nun ja... billig, aber tut es fürs erste
constexpr WORD hashKeywords(const char* string)noexcept{
	size_t size = strlen(string);
	WORD out = 7;
	for(WORD i=0; i < size; ++i){
		out *= 31;
		out += string[i];
	}
	return out;
}

enum OBJKEYWORD{
	OBJ_V = hashKeywords("v"),
	OBJ_VT = hashKeywords("vt"),
	OBJ_VN = hashKeywords("vn"),
	OBJ_F = hashKeywords("f"),
	OBJ_S = hashKeywords("s"),
	OBJ_O = hashKeywords("o"),
	OBJ_G = hashKeywords("g"),
	OBJ_MTLLIB = hashKeywords("mtllib"),
	OBJ_USEMTL = hashKeywords("usemtl"),
	OBJ_COMMENT = hashKeywords("#"),
	OBJ_L = hashKeywords("l")
};

enum MTLKEYWORD{	//TODO hier fehlen noch ein paar
	MTL_NEWMTL = hashKeywords("newmtl"),
	MTL_MAP_KD = hashKeywords("map_Kd"),		//Diffuse
	MTL_MAP_BUMP = hashKeywords("map_Bump"),	//Normal
	MTL_MAP_KA = hashKeywords("map_Ka"),		//AO
	MTL_MAP_KS = hashKeywords("map_Ks"),		//Specular
	MTL_MAP_D = hashKeywords("map_d"),			//TODO Keine Ahnung eine MTL File hat das als map_Kd genutzt
	MTL_MAP_KE = hashKeywords("map_Ke"),
	MTL_COMMENT = hashKeywords("#"),
	MTL_KA = hashKeywords("Ka"),
	MTL_KD = hashKeywords("Kd"),
	MTL_KS = hashKeywords("Ks"),
	MTL_KE = hashKeywords("Ke"),
	MTL_NI = hashKeywords("Ni"),
	MTL_NS = hashKeywords("Ns"),
	MTL_D = hashKeywords("d"),
	MTL_ILLUM = hashKeywords("illum")
};

//Liest ein Wort aus einer Datei wie der << Operator aber gibt zusätzlich zurück, ob \n oder eof gefunden wurde
bool readWord(std::fstream& file, std::string& buffer)noexcept{
	char c;
	buffer.clear();
	bool newline = false;
	c = file.peek();
	while(c == ' ' || c == '\n'){
		file.get();
		c = file.peek();
	}
	while(1){
		c = file.get();
		if(c == ' ') break;
		if(c == '\n'){
			newline = true;
			break;
		}
		if(file.eof()) return true;
		buffer += c;
	}
	while(1){
		c = file.peek();
		if(c == ' '){
			file.get();
			continue;
		}
		if(c == '\n'){
			newline = true;
			file.get();
			continue;
		}
		if(file.eof()) return true;
		break;
	}
	return newline;
}

//Liest so lange Daten in den Buffer, bis ein \n oder eof gefunden wurde
bool readLine(std::fstream& file, std::string& buffer)noexcept{
	buffer.clear();
	int c = file.get();
	while(1){
		if(file.eof()) return true;
		if(c == '\n') return true;
		buffer += c;
	}
}

//Ließt die obj Datei weiter ein bis zum nächsten \n und parsed die Linie basierend auf dem obj keyword, schriebt die Daten in den outData buffer
//und gibt evtl. Fehler zurück
ErrCode parseObjLine(OBJKEYWORD key, std::fstream& file, void* outData)noexcept{
	switch(key){
		case OBJ_VN:
		case OBJ_V:{
			std::string buffer;
			float* data = (float*)outData;
			if(readWord(file, buffer)) return ERR_MODEL_BAD_FORMAT;
			data[0] = std::atof(buffer.c_str());
			if(readWord(file, buffer)) return ERR_MODEL_BAD_FORMAT;
			data[1] = std::atof(buffer.c_str());
			if(!readWord(file, buffer)) return ERR_MODEL_BAD_FORMAT;
			data[2] = std::atof(buffer.c_str());
			break;
		}
		case OBJ_VT:{
			std::string buffer;
			float* data = (float*)outData;
			if(readWord(file, buffer)) return ERR_MODEL_BAD_FORMAT;
			data[0] = std::atof(buffer.c_str());
			if(!readWord(file, buffer)) return ERR_MODEL_BAD_FORMAT;
			data[1] = std::atof(buffer.c_str());
			break;
		}
		case OBJ_S:{	//TODO muss noch implementiert werden
			std::string buffer;
			if(!readWord(file, buffer)) return ERR_MODEL_BAD_FORMAT;
			break;
		}
		case OBJ_F:{
			std::string buffer;
			DWORD* data = (DWORD*)outData;
			if(readWord(file, buffer)) return ERR_MODEL_BAD_FORMAT;
			if(splitString(buffer, data[0], data[1], data[2]) != SUCCESS) return ERR_MODEL_BAD_FORMAT;
			if(readWord(file, buffer)) return ERR_MODEL_BAD_FORMAT;
			if(splitString(buffer, data[3], data[4], data[5]) != SUCCESS) return ERR_MODEL_BAD_FORMAT;
			if(!readWord(file, buffer)) return ERR_MODEL_BAD_FORMAT;
			if(splitString(buffer, data[6], data[7], data[8]) != SUCCESS) return ERR_MODEL_BAD_FORMAT;
			break;
		}
		case OBJ_O:
		case OBJ_G:
		case OBJ_MTLLIB:
		case OBJ_USEMTL:{
			std::string buffer;
			BYTE* data = (BYTE*)outData;
			if(!readWord(file, buffer)) return ERR_MODEL_BAD_FORMAT;
			for(size_t i=0; i < buffer.size(); ++i){
				data[i] = buffer[i];
			}
			data[buffer.size()] = '\0';
			break;
		}
		case OBJ_L:		//TODO muss noch implementiert werden
		case OBJ_COMMENT:{
			std::string buffer;
			BYTE* data = (BYTE*)outData;
			DWORD idx = 0;
			while(!readWord(file, buffer)){
				for(size_t i=0; i < buffer.size(); ++i) data[idx++] = buffer[i];
			}
			for(size_t i=0; i < buffer.size(); ++i) data[idx++] = buffer[i];
			data[idx] = '\0';
			break;
		}
		default: return ERR_MODEL_BAD_FORMAT;
	}
	return SUCCESS;
}

//Ließt die mtl Datei weiter ein bis zum nächsten \n und parsed die Linie basierend auf dem obj keyword, schriebt die daten in den outData buffer
//und gibt evtl. Fehler zurück
//TODO hier fehlen noch ein paar
ErrCode parseMtlLine(MTLKEYWORD key, std::fstream& file, void* outData)noexcept{
	switch(key){
		case MTL_KA:
		case MTL_KD:
		case MTL_KS:
		case MTL_KE:{
			std::string buffer;
			float* data = (float*)outData;
			if(readWord(file, buffer)) return ERR_MODEL_BAD_FORMAT;
			data[0] = std::atof(buffer.c_str());
			if(readWord(file, buffer)) return ERR_MODEL_BAD_FORMAT;
			data[1] = std::atof(buffer.c_str());
			if(!readWord(file, buffer)) return ERR_MODEL_BAD_FORMAT;
			data[2] = std::atof(buffer.c_str());
			break;
		}
		case MTL_D:
		case MTL_NI:
		case MTL_NS:
		case MTL_ILLUM:
		case MTL_NEWMTL:{
			std::string buffer;
			BYTE* data = (BYTE*)outData;
			if(!readWord(file, buffer)) return ERR_MODEL_BAD_FORMAT;
			for(size_t i=0; i < buffer.size(); ++i){
				data[i] = buffer[i];
			}
			data[buffer.size()] = '\0';
			break;
		}
		case MTL_MAP_BUMP:
		case MTL_MAP_KE:
		case MTL_MAP_D:
		case MTL_MAP_KD:
		case MTL_MAP_KS:
		case MTL_MAP_KA:
		case MTL_COMMENT:{
			std::string buffer;
			BYTE* data = (BYTE*)outData;
			DWORD idx = 0;
			while(!readWord(file, buffer)){
				for(size_t i=0; i < buffer.size(); ++i) data[idx++] = buffer[i];
				data[idx++] = ' ';	//TODO EINFACH NUR FALSCH, DAS IST NUR HIER WEIL ICH ZU FAUL WAR MIR EINE BESSERE LÖSUNG FÜR DAS PFAD PROBLEM ZU ÜBERLEGEN
				//Problem ist ein Pfad mit einem Leerzeichen, statt readWord sollte man eine Funktion schreiben die alles bis zum \n einliest, duh
			}
			for(size_t i=0; i < buffer.size(); ++i) data[idx++] = buffer[i];
			data[idx] = '\0';
			break;
		}
		default: return ERR_MODEL_BAD_FORMAT;
	}
	return SUCCESS;
}

//TODO noch nicht alle Keywords werden beachtet
//TODO Theoretisch noch nicht optimal implementiert, da jede Zeile erst in einen Buffer gelesen wird und dieser dann erst geparsed
//TODO Liest aktuell die Texturen in feste Einträge im Texture Array des Materials
ErrCode loadMtl(const char* filename, Material* materials, DWORD& materialCount)noexcept{
	std::fstream file;
	file.open(filename, std::ios::in);
	if(!file.is_open()) return ERR_MATERIAL_NOT_FOUND;
	std::string word;
	void* data[120];			//TODO Manche Texturenpfade könnten länger sein
	DWORD lineNumber = 0;
	while(file >> word){
		lineNumber++;
		MTLKEYWORD key = (MTLKEYWORD)hashKeywords(word.c_str());
		switch(key){
			case MTL_NEWMTL:{
				if(parseMtlLine(key, file, data) != SUCCESS) return ErrCheck(ERR_MATERIAL_BAD_FORMAT, std::string(word + " in Zeile: " + longToString(lineNumber)).c_str());
				materialCount++;
				materials[materialCount-1].name = std::string((char*)data);
				break;
			}
			case MTL_MAP_D:		//Kein Plan ob das richtig ist
			case MTL_MAP_KS:
			case MTL_MAP_KD:{
				BYTE textureIdx = 0;
				switch(key){
					case MTL_MAP_KS:
						textureIdx = 1;
						break;
				}
				if(parseMtlLine(key, file, data) != SUCCESS) return ErrCheck(ERR_MATERIAL_BAD_FORMAT, std::string(word + " in Zeile: " + longToString(lineNumber)).c_str());
				std::string textureFile = std::string((char*)data);
				size_t index = 0;
				while(true){
					index = textureFile.find("\\", index);
					if(index == std::string::npos) break;
					textureFile.replace(index, 2, "/");
					index += 2;
				}
				if(materials[materialCount-1].textures[textureIdx].data == nullptr) destroyImage(materials[materialCount-1].textures[textureIdx]);
				if(textureFile.size() < 4) return ErrCheck(ERR_MATERIAL_BAD_FORMAT, std::string(word + " in Zeile " + longToString(lineNumber)).c_str());
				std::string fileEnding = textureFile.substr(textureFile.size()-4, 4);
				if(fileEnding == ".tex"){
					if(ErrCheck(loadImage(textureFile.c_str(), materials[materialCount-1].textures[textureIdx]), "Texture laden") != SUCCESS) return ErrCheck(ERR_MATERIAL_BAD_FORMAT, std::string(word + " in Zeile: " + longToString(lineNumber)).c_str());
				}
				else if(fileEnding == ".png"){
					if(ErrCheck(loadPng(textureFile.c_str(), materials[materialCount-1].textures[textureIdx]), "Texture laden") != SUCCESS) return ErrCheck(ERR_MATERIAL_BAD_FORMAT, std::string(word + " in Zeile: " + longToString(lineNumber)).c_str());
				}else{
					ErrCheck(ERR_MATERIAL_BAD_FORMAT, std::string(std::string("Nicht unterstütztes Bildformat in Zeile: ") + longToString(lineNumber)).c_str());
				}
				flipImageVertically(materials[materialCount-1].textures[textureIdx]);	//TODO warum nur ist das nötig?
				materials[materialCount-1].textureCount = 1;
				//TODO Das ist nur eine Übergangslösung und sollte besser implementiert werden
				DWORD redBuffer[256]{0};
				DWORD greenBuffer[256]{0};
				DWORD blueBuffer[256]{0};
				DWORD redMax = 0;
				BYTE redIdx = 0;
				DWORD greenMax = 0;
				BYTE greenIdx = 0;
				DWORD blueMax = 0;
				BYTE blueIdx = 0;
				for(DWORD i=0; i < materials[materialCount-1].textures[textureIdx].width*materials[materialCount-1].textures[textureIdx].height; ++i){
					redBuffer[R(materials[materialCount-1].textures[textureIdx].data[i])] += 1;
					greenBuffer[G(materials[materialCount-1].textures[textureIdx].data[i])] += 1;
					blueBuffer[B(materials[materialCount-1].textures[textureIdx].data[i])] += 1;
				}
				for(WORD i=0; i < 256; ++i){
					if(redBuffer[i] > redMax){
						redMax = redBuffer[i];
						redIdx = i;
					}
					if(greenBuffer[i] > greenMax){
						greenMax = greenBuffer[i];
						greenIdx = i;
					}
					if(blueBuffer[i] > blueMax){
						blueMax = blueBuffer[i];
						blueIdx = i;
					}
				}
				materials[materialCount-1].baseColor = RGBA(redIdx, greenIdx, blueIdx);
				break;
			}
			case MTL_KS:
			case MTL_KD:{
				BYTE textureIdx = 0;
				switch(key){
					case MTL_KS:
						textureIdx = 1;
						break;
				}
				if(parseMtlLine(key, file, data) != SUCCESS) return ErrCheck(ERR_MATERIAL_BAD_FORMAT, std::string(word + " in Zeile: " + longToString(lineNumber)).c_str());
				float* values = (float*)data;
				if(materials[materialCount-1].textures[textureIdx].data != nullptr) break;
				if(ErrCheck(createBlankImage(materials[materialCount-1].textures[textureIdx], 4, 4, RGBA(values[0]*255, values[1]*255, values[2]*255)), "Texture laden") != SUCCESS) return ErrCheck(ERR_MATERIAL_BAD_FORMAT, std::string(word + " in Zeile: " + longToString(lineNumber)).c_str());
				materials[materialCount-1].textureCount = 1;
				materials[materialCount-1].baseColor = RGBA(values[0]*255, values[1]*255, values[2]*255);
				break;
			}
			case MTL_MAP_BUMP:	//TODO müssen alle noch implementiert werden
			case MTL_MAP_KE:
			case MTL_NS:
			case MTL_MAP_KA:
			case MTL_KA:
			case MTL_KE:
			case MTL_NI:
			case MTL_D:
			case MTL_ILLUM:
			case MTL_COMMENT:{
				if(parseMtlLine(key, file, data) != SUCCESS) return ErrCheck(ERR_MATERIAL_BAD_FORMAT, std::string(word + " in Zeile: " + longToString(lineNumber)).c_str());
				break;
			}
			default: return ErrCheck(ERR_MATERIAL_BAD_FORMAT, std::string(word + " Unbekanntes MTL Keyword in Zeile: " + longToString(lineNumber)).c_str());
		}
	}
	return SUCCESS;
}

//Speichert die Modelle der .obj Datei und alle .mtl Materialen falls es diese noch nicht gibt 
//TODO man sollte übergeben können in welche location die Attribute gespeichert werden
//TODO unterstützt nur Flächen die aus Dreiecken bestehen
//TODO Theoretisch noch nicht optimal implementiert, da jede Zeile erst in einen Buffer gelesen wird und dieser dann erst geparsed
ErrCode loadObj(const char* filename, TriangleModel* models, DWORD& modelCount, Material* materials, DWORD& materialCount, BYTE attributeCount, float x, float y, float z, float scaleX=1, float scaleY=1, float scaleZ=1)noexcept{
	std::fstream file;
	file.open(filename, std::ios::in);
	if(!file.is_open()) return ERR_MODEL_NOT_FOUND;
	if(models[0].attributesCount < 5) return GENERIC_ERROR;		//TODO Neue Fehlermeldung
	std::string word;
	std::vector<fvec3> points;
	std::vector<fvec3> normals;
	std::vector<fvec2> uvs;
	void* data[80];
	bool hasMaterial = false;
	DWORD lineNumber = 0;
	BYTE windingOrder[] = {0, 1, 2};
	if(((sign(scaleX)+sign(scaleY)+sign(scaleZ))%2) == 0){
		windingOrder[0] = 2;
		windingOrder[2] = 0;
	}
	while(file >> word){
		lineNumber++;
		OBJKEYWORD key = (OBJKEYWORD)hashKeywords(word.c_str());
		switch(key){
			case OBJ_O:{
				if(parseObjLine(key, file, data) != SUCCESS) return ErrCheck(ERR_MODEL_BAD_FORMAT, std::string(word + " in Zeile: " + longToString(lineNumber)).c_str());
				break;
			}
			case OBJ_V:{
				if(parseObjLine(key, file, data) != SUCCESS) return ErrCheck(ERR_MODEL_BAD_FORMAT, std::string(word + " in Zeile: " + longToString(lineNumber)).c_str());
				points.push_back({((float*)data)[0]*scaleX, ((float*)data)[1]*scaleY, ((float*)data)[2]*scaleZ});
				break;
			}
			case OBJ_VN:{
				if(parseObjLine(key, file, data) != SUCCESS) return ErrCheck(ERR_MODEL_BAD_FORMAT, std::string(word + " in Zeile: " + longToString(lineNumber)).c_str());
				normals.push_back({((float*)data)[0], ((float*)data)[1], ((float*)data)[2]});
				break;
			}
			case OBJ_VT:{
				if(parseObjLine(key, file, data) != SUCCESS) return ErrCheck(ERR_MODEL_BAD_FORMAT, std::string(word + " in Zeile: " + longToString(lineNumber)).c_str());
				uvs.push_back({((float*)data)[0], ((float*)data)[1]});
				break;
			}
			case OBJ_F:{
				DWORD pt_order[3];
				DWORD uv_order[3];
				DWORD normal_order[3];

				if(models[modelCount-1].triangleCount >= models[modelCount-1].triangleCapacity) increaseTriangleCapacity(models[modelCount-1], 100);	//TODO 100
				
				//Lese Punkt/Texture/Normal
				if(parseObjLine(key, file, data) != SUCCESS) return ErrCheck(ERR_MODEL_BAD_FORMAT, std::string(word + " in Zeile: " + longToString(lineNumber)).c_str());
				pt_order[0] = ((DWORD*)data)[0]; uv_order[0] = ((DWORD*)data)[1]; normal_order[0] = ((DWORD*)data)[2];
				pt_order[1] = ((DWORD*)data)[3]; uv_order[1] = ((DWORD*)data)[4]; normal_order[1] = ((DWORD*)data)[5];
				pt_order[2] = ((DWORD*)data)[6]; uv_order[2] = ((DWORD*)data)[7]; normal_order[2] = ((DWORD*)data)[8];

				if(modelCount == 0) return ErrCheck(ERR_MODEL_BAD_FORMAT, "f modelCount == 0 aka keine usemtl Zeile vor einer f Zeile");
				DWORD modelIdx = modelCount-1;
				DWORD triangleIdx = models[modelIdx].triangleCount;

				models[modelIdx].triangles[triangleIdx].points[0] = points[pt_order[windingOrder[0]]];
				models[modelIdx].triangles[triangleIdx].points[1] = points[pt_order[windingOrder[1]]];
				models[modelIdx].triangles[triangleIdx].points[2] = points[pt_order[windingOrder[2]]];

				DWORD attributeBaseIdx = triangleIdx*models[modelIdx].attributesCount*3;
				if(uvs.size() > 0){
					models[modelIdx].attributesBuffer[attributeBaseIdx] = uvs[uv_order[windingOrder[0]]].x;
					models[modelIdx].attributesBuffer[attributeBaseIdx+1] = uvs[uv_order[windingOrder[0]]].y;
				}
				models[modelIdx].attributesBuffer[attributeBaseIdx+2] = normals[normal_order[windingOrder[0]]].x*-negSign(scaleX);
				models[modelIdx].attributesBuffer[attributeBaseIdx+3] = normals[normal_order[windingOrder[0]]].y*-negSign(scaleY);	//TODO Warum müssen die negativ sein?
				models[modelIdx].attributesBuffer[attributeBaseIdx+4] = normals[normal_order[windingOrder[0]]].z*-negSign(scaleZ);
				attributeBaseIdx += models[modelIdx].attributesCount;
				if(uvs.size() > 0){
					models[modelIdx].attributesBuffer[attributeBaseIdx] = uvs[uv_order[windingOrder[1]]].x;
					models[modelIdx].attributesBuffer[attributeBaseIdx+1] = uvs[uv_order[windingOrder[1]]].y;
				}
				models[modelIdx].attributesBuffer[attributeBaseIdx+2] = normals[normal_order[windingOrder[1]]].x*-negSign(scaleX);
				models[modelIdx].attributesBuffer[attributeBaseIdx+3] = normals[normal_order[windingOrder[1]]].y*-negSign(scaleY);
				models[modelIdx].attributesBuffer[attributeBaseIdx+4] = normals[normal_order[windingOrder[1]]].z*-negSign(scaleZ);
				attributeBaseIdx += models[modelIdx].attributesCount;
				if(uvs.size() > 0){
					models[modelIdx].attributesBuffer[attributeBaseIdx] = uvs[uv_order[windingOrder[2]]].x;
					models[modelIdx].attributesBuffer[attributeBaseIdx+1] = uvs[uv_order[windingOrder[2]]].y;
				}
				models[modelIdx].attributesBuffer[attributeBaseIdx+2] = normals[normal_order[windingOrder[2]]].x*-negSign(scaleX);
				models[modelIdx].attributesBuffer[attributeBaseIdx+3] = normals[normal_order[windingOrder[2]]].y*-negSign(scaleY);
				models[modelIdx].attributesBuffer[attributeBaseIdx+4] = normals[normal_order[windingOrder[2]]].z*-negSign(scaleZ);

				models[modelIdx].triangleCount++;
				break;
			}
			case OBJ_MTLLIB:{
				if(parseObjLine(key, file, data) != SUCCESS) return ErrCheck(ERR_MODEL_BAD_FORMAT, std::string(word + " in Zeile: " + longToString(lineNumber)).c_str());
				size_t lastSlash = std::string(filename).find_last_of("/");
				std::string path = std::string(filename).substr(0, lastSlash+1);
				std::string mtlFile = path + std::string((char*)data);
				ErrCode err = loadMtl(mtlFile.c_str(), materials, materialCount);
				if(err == ERR_MATERIAL_NOT_FOUND){
					ErrCheck(err, std::string(word + " Material Bibliothek nicht gefunden... fahre ohne fort... in Zeile: " + longToString(lineNumber)).c_str());
					break;
				}
				if(ErrCheck(err, "Mtl Datei laden") != SUCCESS) return ErrCheck(ERR_MODEL_BAD_FORMAT, std::string(word + " in Zeile: " + longToString(lineNumber)).c_str());
				hasMaterial = true;
				break;
			}
			case OBJ_USEMTL:{
				modelCount++;
				if(!hasMaterial){
					if(parseObjLine(key, file, data) != SUCCESS) return ErrCheck(ERR_MODEL_BAD_FORMAT, std::string(word + " in Zeile: " + longToString(lineNumber)).c_str());
					ErrCheck(ERR_MODEL_BAD_FORMAT, std::string(word + " Material angefordert, Datei hat aber keine Material Bibliothek in Zeile: " + longToString(lineNumber)).c_str());
					break;
				}
				if(parseObjLine(key, file, data) != SUCCESS) return ErrCheck(ERR_MODEL_BAD_FORMAT, std::string(word + " in Zeile: " + longToString(lineNumber)).c_str());
				DWORD i=0;
				for(;i < materialCount; ++i){
					if(strcmp((char*)data, materials[i].name.c_str()) == 0){
						models[modelCount-1].material = &materials[i];
						break;
					}
				}
				if(i == materialCount) return ErrCheck(ERR_MODEL_BAD_FORMAT, std::string(word + " Material nicht gefunden in Zeile: " + longToString(lineNumber)).c_str());
				break;
			}
			case OBJ_G:			//TODO müssen alle noch implementiert werden
			case OBJ_S:
			case OBJ_L:
			case OBJ_COMMENT:{
				if(parseObjLine(key, file, data) != SUCCESS) return ErrCheck(ERR_MODEL_BAD_FORMAT, std::string(word + " in Zeile: " + longToString(lineNumber)).c_str());
				break;
			}
			default: return ErrCheck(ERR_MODEL_BAD_FORMAT, std::string(word + " Unbekanntes OBJ Keyword in Zeile: " + longToString(lineNumber)).c_str());
		}
	}
	return SUCCESS;
}

struct uivec3{
    DWORD x;
    DWORD y;
    DWORD z;
};

struct DistanceInfo{
	float distance;
	float w;
	float v;
	float u;
};

void barycentricForEdge(const fvec3& p, const fvec3& a, const fvec3& b, float& u, float& v) {
    fvec3 ab = { b.x - a.x, b.y - a.y, b.z - a.z };
    float abLengthSq = dot(ab, ab);
    float t = clamp(dot({ p.x - a.x, p.y - a.y, p.z - a.z }, ab) / abLengthSq, 0.0f, 1.0f);
    fvec3 closest = { a.x + t * ab.x, a.y + t * ab.y, a.z + t * ab.z };
    u = 1.0f - t;
    v = t;
}

DistanceInfo pointToTriangleDistance(const fvec3& p, Triangle& tri) {
    fvec3 p21 = {tri.points[2].x - tri.points[1].x, tri.points[2].y - tri.points[1].y, tri.points[2].z - tri.points[1].z};
    fvec3 p20 = {tri.points[2].x - tri.points[0].x, tri.points[2].y - tri.points[0].y, tri.points[2].z - tri.points[0].z};
    fvec3 normal = normalize(cross(p21, p20));
    
    fvec3 diff = {p.x - tri.points[0].x, p.y - tri.points[0].y, p.z - tri.points[0].z};
    float dist = dot(normal, diff);
    fvec3 projP = {p.x - normal.x * dist, p.y - normal.y * dist, p.z - normal.z * dist};

    fvec3 v0 = {tri.points[1].x - tri.points[0].x, tri.points[1].y - tri.points[0].y, tri.points[1].z - tri.points[0].z};
    fvec3 v1 = {tri.points[2].x - tri.points[0].x, tri.points[2].y - tri.points[0].y, tri.points[2].z - tri.points[0].z};
    fvec3 v2 = {projP.x - tri.points[0].x, projP.y - tri.points[0].y, projP.z - tri.points[0].z};

    float d00 = dot(v0, v0);
    float d01 = dot(v0, v1);
    float d11 = dot(v1, v1);
    float d20 = dot(v2, v0);
    float d21 = dot(v2, v1);
    float denom = 1.f / (d00 * d11 - d01 * d01);
    float v = (d11 * d20 - d01 * d21) * denom;
    float w = (d00 * d21 - d01 * d20) * denom;
    float u = 1 - v - w;

    if (w >= 0 && v >= 0 && u >= 0) return {fabs(dist), w, v, u};
	#ifdef ACCURATEMESHTOVOXEL
	return {1000, 0, 0, 0};
	#else
    fvec3 p1 = closestPointOnLineSegment(p, tri.points[0], tri.points[1]);
    fvec3 p2 = closestPointOnLineSegment(p, tri.points[0], tri.points[2]);
    fvec3 p3 = closestPointOnLineSegment(p, tri.points[1], tri.points[2]);
    
    float dist1 = length({p1.x - p.x, p1.y - p.y, p1.z - p.z});
    float dist2 = length({p2.x - p.x, p2.y - p.y, p2.z - p.z});
    float dist3 = length({p3.x - p.x, p3.y - p.y, p3.z - p.z});
    
    float minDist = min(dist3, min(dist2, dist1));

    if(minDist == dist1){
        float edgeU, edgeV;
        barycentricForEdge(p, tri.points[0], tri.points[1], edgeU, edgeV);
        return {minDist, 0.0f, edgeV, edgeU};
    }else if (minDist == dist2){
        float edgeU, edgeV;
        barycentricForEdge(p, tri.points[0], tri.points[2], edgeU, edgeV);
        return {minDist, edgeV, 0.0f, edgeU};
    }else{
        float edgeU, edgeV;
        barycentricForEdge(p, tri.points[1], tri.points[2], edgeU, edgeV);
        return {minDist, edgeU, edgeV, 0.0f};
    }
	#endif
}

constexpr DWORD textureRepeated(Image& image, float u, float v)noexcept{
	u = u - floor(u);
	v = v - floor(v);
	WORD u1 = u*(image.width-1);
	WORD v1 = v*(image.height-1);
	return image.data[v1*image.width+u1];
}

void calculateSDFFromMesh(DWORD* sdfData, DWORD dx, DWORD dy, DWORD dz, TriangleModel* models, DWORD modelCount)noexcept{
    float minX = std::numeric_limits<float>::max(), minY = std::numeric_limits<float>::max(), minZ = std::numeric_limits<float>::max();
    float maxX = -std::numeric_limits<float>::max(), maxY = -std::numeric_limits<float>::max(), maxZ = -std::numeric_limits<float>::max();

	for(DWORD i=0; i < modelCount; ++i){
		const TriangleModel& model = models[i];
		for(DWORD j=0; j < model.triangleCount; ++j){
			const Triangle& tri = model.triangles[j];
			for(int k=0; k < 3; ++k){
				const fvec3& point = tri.points[k];
				minX = min(minX, point.x);
				minY = min(minY, point.y);
				minZ = min(minZ, point.z);
				maxX = max(maxX, point.x);
				maxY = max(maxY, point.y);
				maxZ = max(maxZ, point.z);
			}
		}
	}

    float modelSizeX = maxX - minX;
    float modelSizeY = maxY - minY;
    float modelSizeZ = maxZ - minZ;

    float* marked = alloc<float>(dx * dy * dz, "SDF-Gen-MarkedArray");
    for(DWORD i=0; i < dx*dy*dz; ++i) marked[i] = 1000000.f;

	DWORD non_air_voxels = 0;

	for(DWORD i=0; i < modelCount; ++i){
		const TriangleModel& model = models[i];
		for (DWORD j = 0; j < model.triangleCount; ++j) {
			fvec3 point1 = model.triangles[j].points[0];
			fvec3 point2 = model.triangles[j].points[1];
			fvec3 point3 = model.triangles[j].points[2];

			point1.x = (point1.x - minX) * (dx-1) / modelSizeX;
			point1.y = (point1.y - minY) * (dy-1) / modelSizeY;
			point1.z = (point1.z - minZ) * (dz-1) / modelSizeZ;

			point2.x = (point2.x - minX) * (dx-1) / modelSizeX;
			point2.y = (point2.y - minY) * (dy-1) / modelSizeY;
			point2.z = (point2.z - minZ) * (dz-1) / modelSizeZ;

			point3.x = (point3.x - minX) * (dx-1) / modelSizeX;
			point3.y = (point3.y - minY) * (dy-1) / modelSizeY;
			point3.z = (point3.z - minZ) * (dz-1) / modelSizeZ;

			float minSDFX = max(min(point1.x, min(point2.x, point3.x))-1, 0.f);
			float minSDFY = max(min(point1.y, min(point2.y, point3.y))-1, 0.f);
			float minSDFZ = max(min(point1.z, min(point2.z, point3.z))-1, 0.f);
			float maxSDFX = min(max(point1.x, max(point2.x, point3.x))+1, (float)dx-1);
			float maxSDFY = min(max(point1.y, max(point2.y, point3.y))+1, (float)dy-1);
			float maxSDFZ = min(max(point1.z, max(point2.z, point3.z))+1, (float)dz-1);

			for(int x = minSDFX; x <= maxSDFX; ++x){
				for(int y = minSDFY; y <= maxSDFY; ++y){
					for(int z = minSDFZ; z <= maxSDFZ; ++z){
						DWORD idx = z * dy * dx + y * dx + x;
						fvec3 pos = {x * modelSizeX / (dx-1) + minX, y * modelSizeY / (dy-1) + minY, z * modelSizeZ / (dz-1) + minZ};

						DistanceInfo dstInfo = pointToTriangleDistance(pos, model.triangles[j]);

						#ifdef ACCURATEMESHTOVOXEL
						if(dstInfo.distance < 0.2 && dstInfo.distance < marked[idx]){	//TODO < 0.2 ist nicht akkurat, ich tippe mal das kommt daher, dass die distance basierend auf der mesh größe relative zur sdf größe berechnet werden muss
						#else
						if(dstInfo.distance < 2.0 && dstInfo.distance < marked[idx]){	//TODO < 2.0 ist nicht akkurat
						#endif
							float u1 = model.attributesBuffer[j*model.attributesCount*3];
							float v1 = model.attributesBuffer[j*model.attributesCount*3+1];
							float u2 = model.attributesBuffer[j*model.attributesCount*3+model.attributesCount];
							float v2 = model.attributesBuffer[j*model.attributesCount*3+model.attributesCount+1];
							float u3 = model.attributesBuffer[j*model.attributesCount*3+model.attributesCount*2];
							float v3 = model.attributesBuffer[j*model.attributesCount*3+model.attributesCount*2+1];
							float u = dstInfo.u * u1 + dstInfo.v * u2 + dstInfo.w * u3;
							float v = dstInfo.u * v1 + dstInfo.v * v2 + dstInfo.w * v3;
							if(model.material){
								sdfData[idx] = textureRepeated(model.material->textures[0], u, v);
								sdfData[idx] = A(sdfData[idx], 128);
							}
							else sdfData[idx] = RGBA(255, 255, 255, 128);
							marked[idx] = dstInfo.distance;
							non_air_voxels++;
						}
					}
				}
			}
		}
	}
	std::cout << "Nicht leere Voxel: " << non_air_voxels << std::endl;
    dealloc(marked);
}
