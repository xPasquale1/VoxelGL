#include "windowgl.h"

#define ACCURATEMESHTOVOXEL
#include "obj.h"

std::vector<CharData> chars;
std::vector<RectangleData> rectangles;

fvec3 camPos = {0, 0, 0};
float camRotX = PI/2;
float camRotY = 0;
float rotM[9]{
    1, 0, 0,
    0, 1, 0,
    0, 0, 1,
};
bool menuOpen = true;

Checkbox checkboxes[3];

struct GLProgram{
	GLuint program = 0;
	GLuint vertexShader = 0;
	GLuint fragmentShader = 0;
	GLuint computeShader = 0;

	GLProgram(){
		program = glCreateProgram();
	}

	~GLProgram(){
		glDeleteProgram(program);
	}

	ErrCode attachVertexShader(const char* code, const DWORD code_length){
		glDetachShader(program, vertexShader);
		if(ErrCheck(loadShader(vertexShader, GL_VERTEX_SHADER, code, code_length), "Vertex Shader laden") != SUCCESS) return GENERIC_ERROR;
		glAttachShader(program, vertexShader);
		glLinkProgram(program);
		glDeleteShader(vertexShader);
		return SUCCESS;
	}

	ErrCode attachVertexShader(const char* filename){
		glDetachShader(program, vertexShader);
		if(ErrCheck(loadShader(vertexShader, GL_VERTEX_SHADER, filename), "Vertex Shader laden") != SUCCESS) return GENERIC_ERROR;
		glAttachShader(program, vertexShader);
		glLinkProgram(program);
		glDeleteShader(vertexShader);
		return SUCCESS;
	}

	ErrCode attachFragmentShader(const char* code, const DWORD code_length){
		glDetachShader(program, fragmentShader);
		if(ErrCheck(loadShader(fragmentShader, GL_FRAGMENT_SHADER, code, code_length), "Fragment Shader laden") != SUCCESS) return GENERIC_ERROR;
		glAttachShader(program, fragmentShader);
		glLinkProgram(program);
		glDeleteShader(fragmentShader);
		return SUCCESS;
	}

	ErrCode attachFragmentShader(const char* filename){
		glDetachShader(program, fragmentShader);
		if(ErrCheck(loadShader(fragmentShader, GL_FRAGMENT_SHADER, filename), "Fragment Shader laden") != SUCCESS) return GENERIC_ERROR;
		glAttachShader(program, fragmentShader);
		glLinkProgram(program);
		glDeleteShader(fragmentShader);
		return SUCCESS;
	}

	ErrCode attachComputeShader(const char* code, const DWORD code_length){
		glDetachShader(program, computeShader);
		if(ErrCheck(loadShader(computeShader, GL_COMPUTE_SHADER, code, code_length), "Compute Shader laden") != SUCCESS) return GENERIC_ERROR;
		glAttachShader(program, computeShader);
		glLinkProgram(program);
		glDeleteShader(computeShader);
		return SUCCESS;
	}

	ErrCode attachComputeShader(const char* filename){
		glDetachShader(program, computeShader);
		if(ErrCheck(loadShader(computeShader, GL_COMPUTE_SHADER, filename), "Compute Shader laden") != SUCCESS) return GENERIC_ERROR;
		glAttachShader(program, computeShader);
		glLinkProgram(program);
		glDeleteShader(computeShader);
		return SUCCESS;
	}

	void use(){
		glUseProgram(program);
	}
};

struct GBuffer{
	//Framebuffer Objekt
	GLuint framebuffer = 0;
	//Texture Attachments
	GLuint albedo = 0;
	GLuint lighting = 0;
	WORD width, height;

	GBuffer(WORD width, WORD height) : width(width), height(height){
		resize(width, height);
		glGenFramebuffers(1, &framebuffer);
		glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);

		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, albedo, 0);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, lighting, 0);
		GLenum attachments[] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1};
		glDrawBuffers(sizeof(attachments)/sizeof(GLenum), attachments);
	}

	~GBuffer(){
		glDeleteTextures(1, &albedo);
		glDeleteTextures(1, &lighting);
		glDeleteFramebuffers(1, &framebuffer);
	}

	void bind(){
		glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
	}

	void unbind(){
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

	void resize(WORD width, WORD height){
		glDeleteTextures(1, &albedo);
		glDeleteTextures(1, &lighting);
		glGenTextures(1, &albedo);
		glGenTextures(1, &lighting);

		glActiveTexture(GL_TEXTURE5);
		glBindTexture(GL_TEXTURE_2D, albedo);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glActiveTexture(GL_TEXTURE6);
		glBindTexture(GL_TEXTURE_2D, lighting);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

		bind();
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, albedo, 0);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, lighting, 0);
	}
};

GBuffer* globalGBufferRef = nullptr;

struct GPUTimer{
	GLuint timer;
	
	GPUTimer(){glGenQueries(1, &timer);}
	~GPUTimer(){glDeleteQueries(1, &timer);}

	void restart(){glBeginQuery(GL_TIME_ELAPSED, timer);}
	QWORD getTimeNanos(){
		QWORD time_taken;
		glEndQuery(GL_TIME_ELAPSED);
		GLint available = 0;
		while(!available) glGetQueryObjectiv(timer, GL_QUERY_RESULT_AVAILABLE, &available);
		glGetQueryObjectui64v(timer, GL_QUERY_RESULT, &time_taken);
		return time_taken;
	}
	QWORD getTimeMicros(){
		QWORD time_taken;
		glEndQuery(GL_TIME_ELAPSED);
		GLint available = 0;
		while(!available) glGetQueryObjectiv(timer, GL_QUERY_RESULT_AVAILABLE, &available);
		glGetQueryObjectui64v(timer, GL_QUERY_RESULT, &time_taken);
		return time_taken/1000;
	}
	QWORD getTimeMillis(){
		QWORD time_taken;
		glEndQuery(GL_TIME_ELAPSED);
		GLint available = 0;
		while(!available) glGetQueryObjectiv(timer, GL_QUERY_RESULT_AVAILABLE, &available);
		glGetQueryObjectui64v(timer, GL_QUERY_RESULT, &time_taken);
		return time_taken/1000000;
	}
};

struct TreeNode{
	QWORD child_mask;
};

void createSDFLevels(TriangleModel* models, DWORD modelCount, GLuint* texture, GLint dx, GLint dy, GLint dz){
    glGenTextures(3, texture);
    glBindTexture(GL_TEXTURE_3D, texture[0]);

	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
	glPixelStorei(GL_UNPACK_IMAGE_HEIGHT, 0);

    DWORD* sdfData = alloc<DWORD>(dx*dy*dz, "SDF Daten");
	for(DWORD i=0; i < dx*dy*dz; ++i) sdfData[i] = 0;

	calculateSDFFromMesh(sdfData, dx, dy, dz, models, modelCount);
	for(GLint y=0; y < dy; ++y) sdfData[0 * dy * dx + y * dx + 0] = RGBA(255, 255, 255, 128);
	for(GLint y=0; y < dy; ++y) sdfData[(dz-1) * dy * dx + y * dx + 0] = RGBA(255, 255, 255, 128);
	for(GLint y=0; y < dy; ++y) sdfData[0 * dy * dx + y * dx + (dx-1)] = RGBA(255, 255, 255, 128);
	for(GLint y=0; y < dy; ++y) sdfData[(dz-1) * dy * dx + y * dx + (dx-1)] = RGBA(255, 255, 255, 128);

    glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA8UI, dx, dy, dz, 0, GL_BGRA_INTEGER, GL_UNSIGNED_BYTE, sdfData);
	glBindImageTexture(1, texture[0], 0, GL_TRUE, 0, GL_READ_WRITE, GL_RGBA8UI);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

	GLint mipdx8 = std::ceil((float)dx/8);
	GLint mipdy8 = std::ceil((float)dy/8);
	GLint mipdz8 = std::ceil((float)dz/8);
	BYTE* sdfMipMap8 = alloc<BYTE>(mipdx8*mipdy8*mipdz8, "SDF MipMap8");

	for(GLint i=0; i < mipdx8*mipdy8*mipdz8; ++i) sdfMipMap8[i] = 0;
	for(GLint x=0; x < dx; ++x){
		for(GLint y=0; y < dy; ++y){
			for(GLint z=0; z < dz; ++z){
				if(A(sdfData[z * dy * dx + y * dx + x]) > 0) sdfMipMap8[(z/8) * mipdy8 * mipdx8 + (y/8) * mipdx8 + (x/8)] = 255;
			}
		}
	}
	glBindTexture(GL_TEXTURE_3D, texture[1]);
	glTexImage3D(GL_TEXTURE_3D, 0, GL_R8UI, mipdx8, mipdy8, mipdz8, 0, GL_RED_INTEGER, GL_UNSIGNED_BYTE, sdfMipMap8);
	glBindImageTexture(2, texture[1], 0, GL_TRUE, 0, GL_READ_WRITE, GL_R8UI);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

	//Speichernutzung ausgeben
	float total = getMemoryUsageByTag("SDF Daten");
	total += getMemoryUsageByTag("SDF MipMap8");
	const char* sizeNames[] = {"B", "KB", "MB", "GB"};
	BYTE sizeNameIdx = 0;
	while(total >= 1000){
		total /= 1000;
		sizeNameIdx++;
	}
	std::string memoryText = "GPU Memory: ";
	memoryText += floatToString(total);
	memoryText += sizeNames[sizeNameIdx];
	std::cout << memoryText << std::endl;
	//TODO Sollte natürlich deallokiert werden
	// dealloc(sdfData);
	// dealloc(sdfMipMap8);
}

LRESULT CALLBACK windowCallback(HWND, UINT, WPARAM, LPARAM);
INT WinMain(HINSTANCE hInstance, HINSTANCE hPreviousInst, LPSTR lpszCmdLine, int nCmdShow){
	Window window;
    if(ErrCheck(createWindow(window, hInstance, 1000, 1000, 0, 0, 1, "Fenster", windowCallback), "Fenster öffnen") != SUCCESS) return -1;

	GBuffer gBuffer(1000, 1000);
	globalGBufferRef = &gBuffer;

    Font font;
    if(ErrCheck(loadTTF(font, "fonts/OpenSans-Bold.ttf"), "Font laden") != SUCCESS) return -1;

    wglSwapIntervalEXT(0);

    Timer timer;

    GLProgram primaryRaymarchProgram;
    const GLchar vertexShaderCode[] = 
	"#version 330\n"
	"layout(location=0) in vec2 pos;"
	"void main(){"
	"   gl_Position = vec4(pos, 0.0, 1.0);"
	"}";

	if(primaryRaymarchProgram.attachVertexShader(vertexShaderCode, sizeof(vertexShaderCode)) != SUCCESS) return -1;
	if(primaryRaymarchProgram.attachFragmentShader("raymarch.frag") != SUCCESS) return -1;

	GLProgram finalProgram;
	if(finalProgram.attachVertexShader(vertexShaderCode, sizeof(vertexShaderCode)) != SUCCESS) return -1;
	if(finalProgram.attachFragmentShader("final.frag") != SUCCESS) return -1;

	TriangleModel* models = alloc<TriangleModel>(3000, "Triangle-Models Buffer");
	for(DWORD i=0; i < sizeof(models)/sizeof(TriangleModel); ++i) models[i].attributesCount = 8;
	Material* materials = alloc<Material>(400, "Materials Buffer");
	DWORD modelCount = 0;
	DWORD materialCount = 0;
	resetTimer(timer);
	if(ErrCheck(loadObj("objects/bistro.obj", models, modelCount, materials, materialCount, 0, 0, 0, 0, -1, 1, 1), "Modell laden") != SUCCESS) return -1;
	std::cout << "Modell laden: " << getTimerMillis(timer)/1000.f << "s" << std::endl;

	fvec3 modelMin = {0};
	fvec3 modelMax = {0};
	for(DWORD i=0; i < modelCount; ++i){
		for(DWORD j=0; j < models[i].triangleCount; ++j){
			for(BYTE k=0; k < 3; ++k){
				modelMin.x = min(models[i].triangles[j].points[k].x, modelMin.x);
				modelMin.y = min(models[i].triangles[j].points[k].y, modelMin.y);
				modelMin.z = min(models[i].triangles[j].points[k].z, modelMin.z);
				modelMax.x = max(models[i].triangles[j].points[k].x, modelMax.x);
				modelMax.y = max(models[i].triangles[j].points[k].y, modelMax.y);
				modelMax.z = max(models[i].triangles[j].points[k].z, modelMax.z);
			}
		}
	}

	float dx = modelMax.x-modelMin.x;
	float dy = modelMax.y-modelMin.y;
	float dz = modelMax.z-modelMin.z;
	float totalVolume = dx*dy*dz;
	float targetVolume = 1600*1000*1000;
	float scale = std::cbrtf(targetVolume/totalVolume);
	GLint sdfSize[3] = {(GLint)(dx*scale), (GLint)(dy*scale), (GLint)(dz*scale)};
	std::cout << sdfSize[0] << ", " << sdfSize[1] << ", " << sdfSize[2] << std::endl;
	std::cout << "Voxels: " << sdfSize[0]*sdfSize[1]*sdfSize[2] << std::endl;
	camPos = {(float)sdfSize[0]/2, (float)sdfSize[1]/2, (float)sdfSize[2]/2};
	GLuint sdfTextures[3];
	resetTimer(timer);
	createSDFLevels(models, modelCount, sdfTextures, sdfSize[0], sdfSize[1], sdfSize[2]);
	std::cout << "Voxel Daten berechnen: " << getTimerMillis(timer)/1000.f << "s" << std::endl;


	//GI Probes berechnen und Speicher allokieren
	float min_gi_probes = 10000;
	float gi_probe_inv_scale = std::cbrtf((sdfSize[0]*sdfSize[1]*sdfSize[2])/min_gi_probes);
	GLint gi_probes[3] = {GLint(std::ceil(sdfSize[0]/gi_probe_inv_scale)), GLint(std::ceil(sdfSize[1]/gi_probe_inv_scale)), GLint(std::ceil(sdfSize[2]/gi_probe_inv_scale))};
	std::cout << "GI Probes: " << gi_probes[0] << ", " << gi_probes[1] << ", " << gi_probes[2] << " | " << gi_probes[0]*gi_probes[1]*gi_probes[2] << std::endl;

	GLuint gi_probes_ssbo[2];
	glGenBuffers(2, gi_probes_ssbo);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, gi_probes_ssbo[0]);
	const int gi_probe_buffer_size = 4 * 6 * sizeof(float) * gi_probes[0]*gi_probes[1]*gi_probes[2];	//sizeof(vec3) * float[6] * sizeof(float) * Anzahl GI Probes
	float* tmpBuffer = alloc<float>(gi_probe_buffer_size/sizeof(float), "tmp gi probes buffer init data");
	for(int i=0; i < gi_probe_buffer_size/sizeof(float); ++i) tmpBuffer[i] = 0;
	glBufferData(GL_SHADER_STORAGE_BUFFER, gi_probe_buffer_size, tmpBuffer, GL_STATIC_DRAW);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, gi_probes_ssbo[0]);

	glBindBuffer(GL_SHADER_STORAGE_BUFFER, gi_probes_ssbo[1]);
	glBufferData(GL_SHADER_STORAGE_BUFFER, gi_probe_buffer_size, tmpBuffer, GL_STATIC_DRAW);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, gi_probes_ssbo[1]);
	dealloc(tmpBuffer);

	GLProgram compute_gi_probes_program;
	if(compute_gi_probes_program.attachComputeShader("compute_shaders/calculate_probe_lighting.glsl") != SUCCESS) return -1;

	GPUTimer gpu_timer;
	for(int i=0; i < 6; ++i){
		gpu_timer.restart();
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, gi_probes_ssbo[i%2]);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, gi_probes_ssbo[(i+1)%2]);
		compute_gi_probes_program.use();
		GLuint numGroups = std::ceil((gi_probes[0] * gi_probes[1] * gi_probes[2]) / 256.f);
		std::cout << "GI Compute Groups: " << numGroups << std::endl;
		glUniform1i(glGetUniformLocation(compute_gi_probes_program.program, "sdfData"), 1);
		glUniform1i(glGetUniformLocation(compute_gi_probes_program.program, "sdfData4"), 2);
		glUniform3i(glGetUniformLocation(compute_gi_probes_program.program, "sdfSize"), sdfSize[0], sdfSize[1], sdfSize[2]);
		glUniform3i(glGetUniformLocation(compute_gi_probes_program.program, "gi_probe_size"), gi_probes[0], gi_probes[1], gi_probes[2]);
		glDispatchCompute(numGroups, 1, 1);
		glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
		std::cout << "GI-Probes berechnen: " << gpu_timer.getTimeMillis() << "ms" << std::endl;
	}


	GLProgram place_voxel_program;
	if(place_voxel_program.attachComputeShader("compute_shaders/add_emissive_sphere.glsl") != SUCCESS) return -1;

	//Vertex Array erstellen um ein Quad zu zeichnen
    GLuint Verts, VertsVAO;
    glGenVertexArrays(1, &VertsVAO);
    glGenBuffers(1, &Verts);

    glBindVertexArray(VertsVAO);

    float vertices[] = {
        -1, -1,
        -1, 1,
        1, -1,
        1, 1
    };
    glBindBuffer(GL_ARRAY_BUFFER, Verts);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);

	for(auto& x : _memAllocsHints){
		float total = 0;
		for(void* e : x.second){
			total += _memAllocsMap[e].size;
		}
		const char* sizeNames[] = {"B", "KB", "MB", "GB"};
		BYTE sizeNameIdx = 0;
		while(total >= 1000){
			total /= 1000;
			sizeNameIdx++;
		}
		std::string val = x.first + ": ";
		val += floatToString(total);
		val += sizeNames[sizeNameIdx];
		std::cout << val <<  std::endl;
	}

	DWORD timeTaken = 0;
    std::string fpsText = "FPS: 0";
	std::string msText = "0ms";

	checkboxes[0].pos = {10, (WORD)(25+font.pixelSize*4)};
	checkboxes[0].size = {32, 32};
	checkboxes[0].label = "GI";
	checkboxes[1].pos = {10, (WORD)(30+font.pixelSize*4+checkboxes[0].size.y)};
	checkboxes[1].size = {32, 32};
	checkboxes[1].label = "GI 2 Bounces";
	checkboxes[2].pos = {10, (WORD)(35+font.pixelSize*4+checkboxes[0].size.y*2)};
	checkboxes[2].size = {32, 32};
	checkboxes[2].label = "Rebuild GI";

    while(1){
        resetTimer(timer);
        getMessages(window);
        if(getWindowFlag(window, WINDOW_CLOSE)) break;
        clearWindow(window);

		if(!menuOpen){
			if(getButton(mouse, MOUSE_LMB) && !getButton(mouse, MOUSE_PREV_LMB)){
				place_voxel_program.use();
				glUniform1i(glGetUniformLocation(place_voxel_program.program, "sdfData"), 1);
				glUniform1i(glGetUniformLocation(place_voxel_program.program, "sdfData8"), 2);
				glUniform3i(glGetUniformLocation(place_voxel_program.program, "sdfSize"), sdfSize[0], sdfSize[1], sdfSize[2]);
				glUniformMatrix3fv(glGetUniformLocation(place_voxel_program.program, "camRot"), 1, false, rotM);
				glUniform3f(glGetUniformLocation(place_voxel_program.program, "camPos"), camPos.x, camPos.y, camPos.z);
				glDispatchCompute(1, 1, 1);
				glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
			}
		}

		if(getCheckBoxFlag(checkboxes[2], CHECKBOXFLAG_CHECKED)){
			resetCheckBoxFlag(checkboxes[2], CHECKBOXFLAG_CHECKED);
			for(int i=0; i < 6; ++i){
				gpu_timer.restart();
				glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, gi_probes_ssbo[i%2]);
				glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, gi_probes_ssbo[(i+1)%2]);
				compute_gi_probes_program.use();
				GLuint numGroups = std::ceil((gi_probes[0] * gi_probes[1] * gi_probes[2]) / 256.f);
				glUniform1i(glGetUniformLocation(compute_gi_probes_program.program, "sdfData"), 1);
				glUniform1i(glGetUniformLocation(compute_gi_probes_program.program, "sdfData8"), 2);
				glUniform3i(glGetUniformLocation(compute_gi_probes_program.program, "sdfSize"), sdfSize[0], sdfSize[1], sdfSize[2]);
				glUniform3i(glGetUniformLocation(compute_gi_probes_program.program, "gi_probe_size"), gi_probes[0], gi_probes[1], gi_probes[2]);
				glDispatchCompute(numGroups, 1, 1);
				glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
				std::cout << "GI-Probes berechnen: " << gpu_timer.getTimeMillis() << "ms" << std::endl;
			}
		}

        primaryRaymarchProgram.use();
        glBindVertexArray(VertsVAO);
		gBuffer.bind();

		glUniform1i(glGetUniformLocation(primaryRaymarchProgram.program, "sdfData"), 1);
		glUniform1i(glGetUniformLocation(primaryRaymarchProgram.program, "sdfData8"), 2);
		glUniform1i(glGetUniformLocation(primaryRaymarchProgram.program, "gi_enabled"), getCheckBoxFlag(checkboxes[0], CHECKBOXFLAG_CHECKED));
		glUniform1i(glGetUniformLocation(primaryRaymarchProgram.program, "gi_second_bounce"), getCheckBoxFlag(checkboxes[1], CHECKBOXFLAG_CHECKED));
        glUniformMatrix3fv(glGetUniformLocation(primaryRaymarchProgram.program, "camRot"), 1, false, rotM);
        glUniform3f(glGetUniformLocation(primaryRaymarchProgram.program, "camPos"), camPos.x, camPos.y, camPos.z);
        glUniform2f(glGetUniformLocation(primaryRaymarchProgram.program, "windowSize"), window.windowWidth, window.windowHeight);
		glUniform3i(glGetUniformLocation(primaryRaymarchProgram.program, "sdfSize"), sdfSize[0], sdfSize[1], sdfSize[2]);
		glUniform3i(glGetUniformLocation(primaryRaymarchProgram.program, "gi_probe_size"), gi_probes[0], gi_probes[1], gi_probes[2]);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

		gBuffer.unbind();
		finalProgram.use();

		glUniform1i(glGetUniformLocation(finalProgram.program, "albedo"), 5);
		glUniform1i(glGetUniformLocation(finalProgram.program, "lighting"), 6);
		glUniform2f(glGetUniformLocation(finalProgram.program, "windowSize"), window.windowWidth, window.windowHeight);
		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

		float total = getTotalMemoryUsage();
		const char* sizeNames[] = {"B", "KB", "MB", "GB"};
		BYTE sizeNameIdx = 0;
		while(total >= 1000){
			total /= 1000;
			sizeNameIdx++;
		}
		std::string memoryText = "Memory: ";
		memoryText += floatToString(total);
		memoryText += sizeNames[sizeNameIdx];
        drawFontString(window, font, chars, fpsText.c_str(), 10, 10);
		drawFontString(window, font, chars, msText.c_str(), 10, 15+font.pixelSize);
		drawFontString(window, font, chars, memoryText.c_str(), 10, 20+font.pixelSize*2);
		
		if(menuOpen) updateCheckBoxes(window, font, checkboxes, sizeof(checkboxes)/sizeof(Checkbox), rectangles, chars);

		renderRectangles(window, rectangles.data(), rectangles.size());
        renderFontChars(window, font, chars.data(), chars.size());
        drawWindow(window);
        timeTaken = getTimerMicros(timer);
        fpsText = "FPS: " + std::string(longToString(1000000/timeTaken));
		msText = floatToString(timeTaken/1000.f) + " ms";
        rectangles.clear();
		chars.clear();


        if(getButton(mouse, MOUSE_LMB)) setButton(mouse, MOUSE_PREV_LMB);
        else resetButton(mouse, MOUSE_PREV_LMB);

        float cy = std::cos(camRotY);
		float sy = std::sin(camRotY);
		float cx = std::cos(camRotX);
		float sx = std::sin(camRotX);

		float timeDelta = timeTaken/10000.f;

		camPos.x -= getButton(keyboard, KEY_W) * sx * timeDelta;
		camPos.z += getButton(keyboard, KEY_W) * cx * timeDelta;
		camPos.x += getButton(keyboard, KEY_S) * sx * timeDelta;
		camPos.z -= getButton(keyboard, KEY_S) * cx * timeDelta;
		camPos.x -= getButton(keyboard, KEY_A) * cx * timeDelta;
		camPos.z -= getButton(keyboard, KEY_A) * sx * timeDelta;
		camPos.x += getButton(keyboard, KEY_D) * cx * timeDelta;
		camPos.z += getButton(keyboard, KEY_D) * sx * timeDelta;
		camPos.y -= getButton(keyboard, KEY_SHIFT) * timeDelta;
		camPos.y += getButton(keyboard, KEY_SPACE) * timeDelta;

        float rotMX[] = {
            1, 0, 0,
			0, cy, -sy,
			0, sy, cy,
        };
        float rotMY[] = {
            cx, 0, sx,
			0, 1, 0,
			-sx, 0, cx,
        };
        mult3x3Mat(rotMX, rotMY, rotM);
    }

    destroyFont(font);
    if(ErrCheck(destroyWindow(window), "Fenster schließen") != SUCCESS) return -1;
    return 0;
}

LRESULT CALLBACK windowCallback(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam){
	Window* window = (Window*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
	if(window == nullptr || window->handle == NULL) return DefWindowProc(hwnd, uMsg, wParam, lParam);	//TODO das ist ein Fehler, wie melden aber?
	switch(uMsg){
		case WM_CLOSE:
		case WM_DESTROY:{
			ErrCheck(setWindowFlag(*window, WINDOW_CLOSE), "setze close Fensterstatus");
			return 0;
		}
		case WM_SIZE:{
			UINT width = LOWORD(lParam);
			UINT height = HIWORD(lParam);
			if(!width || !height) break;
			ErrCheck(setWindowFlag(*window, WINDOW_RESIZE), "setzte resize Fensterstatus");
			ErrCheck(resizeWindow(*window, width, height, 1), "Fenster skalieren");
			if(globalGBufferRef) globalGBufferRef->resize(width, height);
			break;
		}
		case WM_LBUTTONDOWN:{
			setButton(mouse, MOUSE_LMB);
			break;
		}
		case WM_LBUTTONUP:{
			resetButton(mouse, MOUSE_LMB);
			break;
		}
		case WM_RBUTTONDOWN:{
			setButton(mouse, MOUSE_RMB);
			break;
		}
		case WM_RBUTTONUP:{
			resetButton(mouse, MOUSE_RMB);
			break;
		}
		case WM_MOUSEMOVE:{
			tagPOINT m_pos;
			tagRECT w_pos;
			GetCursorPos(&m_pos);
			GetWindowRect(hwnd, &w_pos);
			POINT point = {0, 0};
			ClientToScreen(hwnd, &point);
			mouse.x = (m_pos.x - w_pos.left);
			mouse.y = ((m_pos.y - w_pos.top)-(point.y-w_pos.top));
			if(!menuOpen){
				camRotX -= ((float)m_pos.x-(window->windowWidth/2+w_pos.left)) * 0.001;
				camRotY -= ((float)m_pos.y-(window->windowHeight/2+w_pos.top)) * 0.001;
				camRotY = clamp(camRotY, (float)-PI/2, (float)PI/2);
				SetCursorPos(window->windowWidth/2+w_pos.left, window->windowHeight/2+w_pos.top);
			}
			break;
		}
        case WM_KEYDOWN:{
			switch(wParam){
			case 0x57:	//W
				setButton(keyboard, KEY_W);
				break;
			case 0x53:	//S
				setButton(keyboard, KEY_S);
				break;
			case 0x44:	//D
				setButton(keyboard, KEY_D);
				break;
			case 0x41:	//A
				setButton(keyboard, KEY_A);
				break;
			case VK_ESCAPE:
				menuOpen = !menuOpen;
				break;
			case VK_SPACE:
				setButton(keyboard, KEY_SPACE);
				break;
			case VK_SHIFT:
				setButton(keyboard, KEY_SHIFT);
				break;
			case VK_UP:
				setButton(keyboard, KEY_UP);
				break;
			case VK_DOWN:
				setButton(keyboard, KEY_DOWN);
				break;
			case VK_LEFT:
				setButton(keyboard, KEY_LEFT);
				break;
			case VK_RIGHT:
				setButton(keyboard, KEY_RIGHT);
				break;
			}
			return 0L;
		}
		case WM_KEYUP:{
			switch(wParam){
			case 0x57:	//W
				resetButton(keyboard, KEY_W);
				break;
			case 0x53:	//S
				resetButton(keyboard, KEY_S);
				break;
			case 0x44:	//D
				resetButton(keyboard, KEY_D);
				break;
			case 0x41:	//A
				resetButton(keyboard, KEY_A);
				break;
			case VK_SPACE:
				resetButton(keyboard, KEY_SPACE);
				break;
			case VK_SHIFT:
				resetButton(keyboard, KEY_SHIFT);
				break;
			case VK_UP:
				resetButton(keyboard, KEY_UP);
				break;
			case VK_DOWN:
				resetButton(keyboard, KEY_DOWN);
				break;
			case VK_LEFT:
				resetButton(keyboard, KEY_LEFT);
				break;
			case VK_RIGHT:
				resetButton(keyboard, KEY_RIGHT);
				break;
			}
			return 0L;
		}
	}
	return DefWindowProc(hwnd, uMsg, wParam, lParam);
}
