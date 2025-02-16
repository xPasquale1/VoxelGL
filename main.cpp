#include "../OpenGL-Library/windowgl.h"
#include "../OpenGL-Library/font.h"

#define ACCURATEMESHTOVOXEL
#include "obj.h"

std::vector<CharData> chars;

fvec3 camPos = {0, 0, 0};
float camRotX = 0;
float camRotY = 0;
float rotM[9]{
    1, 0, 0,
    0, 1, 0,
    0, 0, 1,
};
bool menuOpen = true;

struct AABB{
	fvec3 min;
	fvec3 max;
};

void createSDFLevels(TriangleModel* models, DWORD modelCount, GLuint* texture, GLint dx, GLint dy, GLint dz){
    glGenTextures(2, texture);
	glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_3D, texture[0]);

    DWORD* sdfData = alloc<DWORD>(dx*dy*dz, "SDF Daten");
	for(DWORD i=0; i < dx*dy*dz; ++i) sdfData[i] = 0;

	calculateSDFFromMesh(sdfData, dx, dy, dz, models, modelCount);
	for(GLint y=0; y < dy; ++y) sdfData[0 * dy * dx + y * dx + 0] = RGBA(255, 255, 255, 128);
	for(GLint y=0; y < dy; ++y) sdfData[(dz-1) * dy * dx + y * dx + 0] = RGBA(255, 255, 255, 128);
	for(GLint y=0; y < dy; ++y) sdfData[0 * dy * dx + y * dx + (dx-1)] = RGBA(255, 255, 255, 128);
	for(GLint y=0; y < dy; ++y) sdfData[(dz-1) * dy * dx + y * dx + (dx-1)] = RGBA(255, 255, 255, 128);

    glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA8, dx, dy, dz, 0, GL_BGRA, GL_UNSIGNED_BYTE, sdfData);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	GLint mipdx = dx/8;
	if(dx%8) mipdx += 2;
	GLint mipdy = dy/8;
	if(dy%8) mipdy += 2;
	GLint mipdz = dz/8;
	if(dz%8) mipdz += 2;
	BYTE* sdfMipMap = alloc<BYTE>(mipdx*mipdy*mipdz, "SDF MipMap");

	float total = getMemoryUsageByTag("SDF Daten");
	total += getMemoryUsageByTag("SDF MipMap");
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

	for(GLint i=0; i < mipdx*mipdy*mipdz; ++i) sdfMipMap[i] = 0;
	for(GLint x=0; x < dx; ++x){
		for(GLint y=0; y < dy; ++y){
			for(GLint z=0; z < dz; ++z){
				if(A(sdfData[z * dy * dx + y * dx + x]) > 0) sdfMipMap[(z/8) * mipdy * mipdx + (y/8) * mipdx + (x/8)] = 255;
			}
		}
	}
	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_3D, texture[1]);
	glTexImage3D(GL_TEXTURE_3D, 0, GL_R8, mipdx, mipdy, mipdz, 0, GL_RED, GL_UNSIGNED_BYTE, sdfMipMap);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	// dealloc(sdfData);
	// dealloc(sdfMipMap);
}

LRESULT CALLBACK windowCallback(HWND, UINT, WPARAM, LPARAM);
INT WinMain(HINSTANCE hInstance, HINSTANCE hPreviousInst, LPSTR lpszCmdLine, int nCmdShow){
	Window window;
    if(ErrCheck(createWindow(window, hInstance, 1000, 1000, 0, 0, 1, "Fenster", windowCallback), "Fenster öffnen") != SUCCESS) return -1;
    if(init() != SUCCESS) return -1;

    Font font;
    if(ErrCheck(loadTTF(font, "fonts/OpenSans-Bold.ttf"), "Font laden") != SUCCESS) return -1;

    wglSwapIntervalEXT(0);

    Timer timer;

    GLint program = glCreateProgram();
    const GLchar vertexShaderCode[] = 
	"#version 330\n"
	"layout(location=0) in vec2 pos;"
	"void main(){"
	"   gl_Position = vec4(pos, 0.0, 1.0);"
	"}";

    GLuint vertexShader = 0;
    GLuint fragmentShader = 0;
	GLuint geometryShader = 0;
    if(ErrCheck(loadShader(vertexShader, GL_VERTEX_SHADER, vertexShaderCode, sizeof(vertexShaderCode)), "Vertex Shader laden") != SUCCESS) return GENERIC_ERROR;
    if(ErrCheck(loadShader(fragmentShader, GL_FRAGMENT_SHADER, "raymarch.frag"), "Fragment Shader laden") != SUCCESS) return GENERIC_ERROR;
    glAttachShader(program, vertexShader);
	glAttachShader(program, fragmentShader);
    glLinkProgram(program);
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

	TriangleModel models[50];
	for(DWORD i=0; i < sizeof(models)/sizeof(TriangleModel); ++i) models[i].attributesCount = 8;
	Material materials[50];
	DWORD modelCount = 0;
	DWORD materialCount = 0;
	if(ErrCheck(loadObj("objects/sponza.obj", models, modelCount, materials, materialCount, 0, 0, 0, 0), "Modell laden") != SUCCESS) return -1;

	GLint sdfSize[3] = {1472, 640, 880};
	// GLint sdfSize[3] = {1600, 490, 1304};
	camPos = {(float)sdfSize[0]/2, (float)sdfSize[1]/2, (float)sdfSize[2]/2};
	GLuint sdfTextures[2];
	createSDFLevels(models, modelCount, sdfTextures, sdfSize[0], sdfSize[1], sdfSize[2]);

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

    while(1){
        resetTimer(timer);
        getMessages(window);
        if(getWindowFlag(window, WINDOW_CLOSE)) break;
        clearWindow(window);

        glUseProgram(program);
        glBindVertexArray(VertsVAO);

        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_3D, sdfTextures[0]);
		glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_3D, sdfTextures[1]);
		glUniform1i(glGetUniformLocation(program, "sdfData"), 1);
		glUniform1i(glGetUniformLocation(program, "sdfDataLow"), 2);
        glUniformMatrix3fv(glGetUniformLocation(program, "camRot"), 1, false, rotM);
        glUniform3f(glGetUniformLocation(program, "camPos"), camPos.x, camPos.y, camPos.z);
        glUniform2f(glGetUniformLocation(program, "windowSize"), window.windowWidth, window.windowHeight);
		glUniform3i(glGetUniformLocation(program, "sdfSize"), sdfSize[0], sdfSize[1], sdfSize[2]);
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

        renderFontChars(window, font, chars.data(), chars.size());
        drawWindow(window);
        timeTaken = getTimerMicros(timer);
        fpsText = "FPS: " + std::string(longToString(1000000/timeTaken));
		msText = floatToString(timeTaken/1000.f) + " ms";
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
