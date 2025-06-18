#pragma once

#include <iostream>
#include <vector>
#include <math.h>
#include <unordered_map>
#include <list>

typedef unsigned char BYTE;			//8  Bit
typedef signed char SBYTE;			//8  Bit signed
typedef unsigned short WORD;		//16 Bit
typedef signed short SWORD;			//16 Bit signed
typedef unsigned long DWORD;		//32 Bit
typedef signed long SDWORD;			//32 Bit signed
typedef unsigned long long QWORD;	//64 Bit
typedef signed long long SQWORD;	//64 Bit signed

/*	TODOs Für das Speicher allokieren/löschen Zeugs hier:
	1: Es ist beschissen.
	2: Einzelne Varariablen werden mit new[] angelegt, was unnötiger Overhead sein kann
	3: Aktuell werden Listen verwendet, Vektoren könnten dennoch schneller/besser sein
	4: Es ist gar nicht Threadsicher...
	5: Wenn man den Typ castet vor dem dealloc wird bestimmt die Welt untergehen
	6: Parameter werden nicht an die Kontruktoren übergeben
	7: Keine Initialiserliste
	8: Tags Einträge werden nie gelöscht... Ironisch ein Memory Tracker mit Memory Leak (:
*/

#define TRACKMEMORY

#ifdef TRACKMEMORY
struct AllocInfo{
    std::list<void*>::iterator ptr;
    std::list<void*>* list;
    QWORD size;
};
std::unordered_map<void*, AllocInfo> _memAllocsMap;
std::unordered_map<std::string, std::list<void*>> _memAllocsHints;
#endif

template<typename T>
T* alloc(QWORD count, const std::string& tag = "Unknown")noexcept{
	T* ptr = new(std::nothrow) T[count];
    #ifdef TRACKMEMORY
    if(ptr){
        auto& list = _memAllocsHints[tag];
        std::list<void*>::iterator ref = list.insert(list.end(), ptr);
        AllocInfo& element = _memAllocsMap[ptr];
        element.size = count*sizeof(T);
        element.ptr = ref;
        element.list = &list;
    }
    #endif
    return ptr;
}

template <typename T>
void dealloc(T* ptr)noexcept{
	delete[] ptr;
    #ifdef TRACKMEMORY
	if(!ptr) return;
    AllocInfo& element = _memAllocsMap[ptr];
    element.list->erase(element.ptr);
    _memAllocsMap.erase(ptr);
    #endif
}

QWORD getTotalMemoryUsage()noexcept{
    QWORD total = 0;
    #ifdef TRACKMEMORY
    for(const auto& it : _memAllocsMap){
        total += it.second.size;
    }
    #endif
    return total;
}

QWORD getMemoryUsageByTag(const std::string& tag)noexcept{
    QWORD total = 0;
    #ifdef TRACKMEMORY
    std::list<void*> list = _memAllocsHints[tag];
    for(const auto& ptr : list){
        total += _memAllocsMap[ptr].size;
    }
    #endif
    return total;
}

enum MOUSEBUTTON{
	MOUSE_LMB = 1,
	MOUSE_RMB = 2,
	MOUSE_PREV_LMB = 4,
	MOUSE_PREV_RMB = 8
};
struct Mouse{
	WORD x;
	WORD y;
	char button = 0;	//Bits: LMB, RMB, Rest ungenutzt
}; static Mouse mouse;

constexpr bool getButton(Mouse& mouse, MOUSEBUTTON button){return (mouse.button & button);}
constexpr void setButton(Mouse& mouse, MOUSEBUTTON button){mouse.button |= button;}
constexpr void resetButton(Mouse& mouse, MOUSEBUTTON button){mouse.button &= ~button;}

constexpr const char* stringLookUp2(long value){
	return &"001020304050607080900111213141516171819102122232425262728292"
			"031323334353637383930414243444546474849405152535455565758595"
			"061626364656667686960717273747576777879708182838485868788898"
			"09192939495969798999"[value<<1];
}
//std::to_string ist langsam, das ist simpel und schnell
//TODO Nicht Multithreading safe
static char _dec_to_str_out[12] = "0000000000\0";
const char* longToString(long value){
	char* ptr = _dec_to_str_out + 10;
	char c = 0;
	if(value < 0){
		c = '-';
		value = 0-value;
	}
	while(value >= 100){
		const char* tmp = stringLookUp2(value%100);
		ptr[0] = tmp[0];
		ptr[-1] = tmp[1];
		ptr -= 2;
		value /= 100;
	}
	while(value){
		*ptr-- = '0'+value%10;
		value /= 10;
	}
	if(c) *ptr-- = c;
	return ptr+1;
}

//value hat decimals Nachkommestellen
//TODO das sieht ein bisschen zu komplex aus für das was es eigentlich machen sollte
std::string intToString(int value, BYTE decimals=2){
	std::string out = longToString(value);
	if(out.size() < ((size_t)decimals+1) && out[0] != '-') out.insert(0, (decimals+1)-out.size(), '0');
	else if(out.size() < ((size_t)decimals+2) && out[0] == '-') out.insert(1, (decimals+1)-(out.size()-1), '0');
	if(decimals) out.insert(out.size()-decimals, 1, '.');
	return out;
}

//TODO sollte Fälle wie NAN, INF,... zuvor testen
std::string floatToString(float value, BYTE decimals=2){
	WORD precision = pow(10, decimals);
	long val = value * precision;
	return intToString(val, decimals);
}

float stringToFloat(const char* str)noexcept{
	float result = 0.0f;
    float factor = 1.0f;
    int sign = 1;
    
    while(*str == ' ') str++;

    if(*str == '-'){
        sign = -1;
        str++;
	}

    while(*str >= '0' && *str <= '9'){
        result = result * 10.0f + (*str - '0');
        str++;
    }

    if(*str == '.'){
        str++;
        while(*str >= '0' && *str <= '9'){
            factor *= 0.1f;
            result += (*str - '0') * factor;
            str++;
        }
    }

    return result * sign;
}

std::string memoryUsageToHuman(QWORD bytes)noexcept{
	float total = bytes;
	const char* sizeNames[] = {"B", "KB", "MB", "GB", "TB"};
	BYTE sizeNameIdx = 0;
	while(total >= 1000){
		total /= 1000;
		sizeNameIdx++;
	}
	std::string memoryText;
	memoryText += floatToString(total);
	memoryText += ' ';
	memoryText += sizeNames[sizeNameIdx];
	return memoryText;
}

//Error-Codes
enum ErrCode{
	//Fenster
	SUCCESS = 0,
	GENERIC_ERROR,
	APP_INIT,
	BAD_ALLOC,
	CREATE_WINDOW,
	TEXTURE_NOT_FOUND,
	MODEL_NOT_FOUND,
	MODEL_BAD_FORMAT,
	FILE_NOT_FOUND,
	WINDOW_NOT_FOUND,
	INIT_RENDER_TARGET,
	//USB
	INVALID_USB_HANDLE,
	COMMSTATE_ERROR,
	TIMEOUT_SET_ERROR,
	//Intern
	OPEN_FILE,
	//Obj
	ERR_MODEL_NOT_FOUND,
	ERR_MATERIAL_NOT_FOUND,
	ERR_MODEL_BAD_FORMAT,
	ERR_MATERIAL_BAD_FORMAT
};
enum ErrCodeFlags{
	ERR_NO_FLAG = 0,
	ERR_NO_OUTPUT = 1,
	ERR_ON_FATAL = 2
};
std::string getTimeString()noexcept{
	SYSTEMTIME time;
	GetLocalTime(&time);
	std::string msg;
	msg += longToString(time.wHour);
	msg += ":"; 
	msg += longToString(time.wMinute);
	msg += ":"; 
	msg += longToString(time.wSecond);
	msg += ":";
	msg += longToString(time.wMilliseconds);
	return msg;
}
//TODO ERR_ON_FATAL ausgeben können wenn der nutzer es so möchte
ErrCode ErrCheck(ErrCode code, const char* msg="\0", ErrCodeFlags flags=ERR_NO_FLAG){
	std::string timeStr = getTimeString();
	switch(code){
	case BAD_ALLOC:
		if(!(flags&ERR_NO_OUTPUT)) std::cerr << timeStr << " [BAD_ALLOC ERROR] " << msg << std::endl;
		return code;
	case GENERIC_ERROR:
		if(!(flags&ERR_NO_OUTPUT)) std::cerr << timeStr << " [GENERIC_ERROR ERROR] " << msg << std::endl;
		return code;
	case CREATE_WINDOW:
		if(!(flags&ERR_NO_OUTPUT)) std::cerr << timeStr << " [CREATE_WINDOW ERROR] " << msg << std::endl;
		return code;
	case TEXTURE_NOT_FOUND:
		if(!(flags&ERR_NO_OUTPUT)) std::cerr << timeStr << " [TEXTURE_NOT_FOUND ERROR] " << msg << std::endl;
		return code;
	case ERR_MODEL_NOT_FOUND:
	case MODEL_NOT_FOUND:
		if(!(flags&ERR_NO_OUTPUT)) std::cerr << timeStr << " [MODEL_NOT_FOUND ERROR] " << msg << std::endl;
		return code;
	case ERR_MODEL_BAD_FORMAT:
	case MODEL_BAD_FORMAT:
		if(!(flags&ERR_NO_OUTPUT)) std::cerr << timeStr << " [MODEL_BAD_FORMAT ERROR] " << msg << std::endl;
		return code;
	case ERR_MATERIAL_NOT_FOUND:
		if(!(flags&ERR_NO_OUTPUT)) std::cerr << timeStr << " [ERR_MATERIAL_NOT_FOUND ERROR] " << msg << std::endl;
		return code;
	case ERR_MATERIAL_BAD_FORMAT:
		if(!(flags&ERR_NO_OUTPUT)) std::cerr << timeStr << " [ERR_MATERIAL_BAD_FORMAT ERROR] " << msg << std::endl;
		return code;
	case FILE_NOT_FOUND:
		if(!(flags&ERR_NO_OUTPUT)) std::cerr << timeStr << " [FILE_NOT_FOUND ERROR] " << msg << std::endl;
		return code;
	case WINDOW_NOT_FOUND:
		if(!(flags&ERR_NO_OUTPUT)) std::cerr << timeStr << " [WINDOW_NOT_FOUND ERROR] " << msg << std::endl;
		return code;
	case APP_INIT:
		if(!(flags&ERR_NO_OUTPUT)) std::cerr << timeStr << " [APP_INIT ERROR] " << msg << std::endl;
		return code;
	case INIT_RENDER_TARGET:
		if(!(flags&ERR_NO_OUTPUT)) std::cerr << timeStr << " [INIT_RENDER_TARGET ERROR] " << msg << std::endl;
		return code;
	case INVALID_USB_HANDLE:
		if(!(flags&ERR_NO_OUTPUT)) std::cerr << timeStr << " [INVALID_USB_HANDLE ERROR] " << msg << std::endl;
		return code;
	case COMMSTATE_ERROR:
		if(!(flags&ERR_NO_OUTPUT)) std::cerr << timeStr << " [COMMSTATE_ERROR ERROR] " << msg << std::endl;
		return code;
	case TIMEOUT_SET_ERROR:
		if(!(flags&ERR_NO_OUTPUT)) std::cerr << timeStr << " [TIMEOUT_SET_ERROR ERROR] " << msg << std::endl;
		return code;
	case OPEN_FILE:
		if(!(flags&ERR_NO_OUTPUT)) std::cerr << timeStr << " [OPEN_FILE ERROR] " << msg << std::endl;
		return code;
	default: return SUCCESS;
	}
	return SUCCESS;
}

enum KEYBOARDBUTTON : QWORD{
	KEY_NONE = 0ULL,
	KEY_0 = 0ULL,
	KEY_1 = 1ULL << 1,
	KEY_3 = 1ULL << 2,
	KEY_4 = 1ULL << 3,
	KEY_5 = 1ULL << 4,
	KEY_6 = 1ULL << 5,
	KEY_7 = 1ULL << 6,
	KEY_8 = 1ULL << 7,
	KEY_9 = 1ULL << 8,
	KEY_A = 1ULL << 9,
	KEY_B = 1ULL << 10,
	KEY_C = 1ULL << 11,
	KEY_D = 1ULL << 12,
	KEY_E = 1ULL << 13,
	KEY_F = 1ULL << 14,
	KEY_G = 1ULL << 15,
	KEY_H = 1ULL << 16,
	KEY_I = 1ULL << 17,
	KEY_J = 1ULL << 18,
	KEY_K = 1ULL << 19,
	KEY_L = 1ULL << 20,
	KEY_M = 1ULL << 21,
	KEY_N = 1ULL << 22,
	KEY_O = 1ULL << 23,
	KEY_P = 1ULL << 24,
	KEY_Q = 1ULL << 25,
	KEY_R = 1ULL << 26,
	KEY_S = 1ULL << 27,
	KEY_T = 1ULL << 28,
	KEY_U = 1ULL << 29,
	KEY_V = 1ULL << 30,
	KEY_W = 1ULL << 31,
	KEY_X = 1ULL << 32,
	KEY_Y = 1ULL << 33,
	KEY_Z = 1ULL << 34,
	KEY_SHIFT = 1ULL << 35,
	KEY_SPACE = 1ULL << 36,
	KEY_CTRL = 1ULL << 37,
	KEY_ALT = 1ULL << 38,
	KEY_ESC = 1ULL << 39,
	KEY_TAB = 1ULL << 40,
	KEY_ENTER = 1ULL << 41,
	KEY_BACK = 1ULL << 42,
	KEY_UP = 1ULL << 43,
	KEY_DOWN = 1ULL << 44,
	KEY_LEFT = 1ULL << 45,
	KEY_RIGHT = 1ULL << 46
};
struct Keyboard{
	QWORD buttons;	//Bits siehe enum oben
}; static Keyboard keyboard;

constexpr bool getButton(Keyboard& keyboard, KEYBOARDBUTTON button){return keyboard.buttons & button;}
constexpr void setButton(Keyboard& keyboard, KEYBOARDBUTTON button){keyboard.buttons |= button;}
constexpr void resetButton(Keyboard& keyboard, KEYBOARDBUTTON button){keyboard.buttons &= ~button;}

struct Timer{
	LARGE_INTEGER startTime;
	LARGE_INTEGER frequency;
};

//Setzt den Startzeitpunkt des Timers zurück
void resetTimer(Timer& timer)noexcept{
	QueryPerformanceFrequency(&timer.frequency); 
	QueryPerformanceCounter(&timer.startTime);
}
//Gibt den Zeitunterschied seid dem Startzeitpunkt in Millisekunden zurück
QWORD getTimerMillis(Timer& timer)noexcept{
	LARGE_INTEGER endTime;
	QueryPerformanceCounter(&endTime);
	LONGLONG timediff = endTime.QuadPart - timer.startTime.QuadPart;
	timediff *= 1000;
	return (timediff / timer.frequency.QuadPart);
}
//Gibt den Zeitunterschied seid dem Startzeitpunkt in Mikrosekunden zurück
QWORD getTimerMicros(Timer& timer)noexcept{
	LARGE_INTEGER endTime;
	QueryPerformanceCounter(&endTime);
	LONGLONG timediff = endTime.QuadPart - timer.startTime.QuadPart;
	timediff *= 1000000;
	return (timediff / timer.frequency.QuadPart);
}
//Gibt den Zeitunterschied seid dem Startzeitpunkt in "Nanosekunden" zurück
//(leider hängt alles von QueryPerformanceFrequency() ab, also kann es sein, dass man nur Intervalle von Nanosekunden bekommt)
QWORD getTimerNanos(Timer& timer)noexcept{
	LARGE_INTEGER endTime;
	QueryPerformanceCounter(&endTime);
	LONGLONG timediff = endTime.QuadPart - timer.startTime.QuadPart;
	timediff *= 1000000000;
	return (timediff / timer.frequency.QuadPart);
}

//TODO Braucht noch eine destroy Funktion
template<typename T>
struct AverageCalculator{
	T* buffer = nullptr;
	WORD bufferSize = 0;
	WORD idx = 0;
};

template<typename T>
ErrCode createAverageCalculator(AverageCalculator<T>& calculator, WORD bufferSize)noexcept{
	calculator.buffer = alloc<T>(bufferSize, "Avg-Calculator-Buffer");
	if(!calculator.buffer) return BAD_ALLOC;
	calculator.bufferSize = bufferSize;
	calculator.idx = 0;
	return SUCCESS;
}

template<typename T>
void insertValue(AverageCalculator<T>& calculator, T value)noexcept{
	calculator.buffer[calculator.idx] = value;
	calculator.idx = (calculator.idx+1)%calculator.bufferSize;
}

template<typename T>
T getAverage(AverageCalculator<T>& calculator)noexcept{
	T average;
	for(WORD i=0; i < calculator.bufferSize; ++i){
		average += calculator.buffer[i];
	}
	return average/calculator.bufferSize;
}

//TODO Sollte ein Template sein
struct HashmapData{
	HashmapData* next = nullptr;
	DWORD key;
	void* data = nullptr;
};

//Bildet eine DWORD key auf einen generischen void* ab
struct Hashmap{
	DWORD tableSize = 0;
	DWORD size = 0;
	HashmapData** tableBuffer = nullptr;
};

ErrCode createHashmap(Hashmap& map, DWORD tableSize=800)noexcept{
	map.tableSize = tableSize;
	map.tableBuffer = alloc<HashmapData*>(tableSize, "Hashmap-Buffer");
	for(DWORD i=0; i < tableSize; ++i) map.tableBuffer[i] = nullptr;
	if(!map.tableBuffer) return BAD_ALLOC;
	return SUCCESS;
}

//Löscht die Hashmap und deallokiert alle intern allokierten Ressourcen
void destroyHashmap(Hashmap& map)noexcept{
	for(DWORD i=0; i < map.tableSize; ++i){
		HashmapData* current = map.tableBuffer[i];
		while(current != nullptr){
			HashmapData* toDelete = current;
			current = current->next;
			dealloc(toDelete);
		}
	}
	dealloc(map.tableBuffer);
	map.size = 0;
}

//Pointer für den Schlüssel key falls gefunden, sonst nullptr
void* searchHashmap(Hashmap& map, DWORD key)noexcept{
	DWORD idx = key % map.tableSize;
	HashmapData* current = map.tableBuffer[idx];
	while(current != nullptr){
		if(current->key == key) return current->data;
		current = current->next;
	}
	return nullptr;
}

//Fügt das Schlüssel-Wert-Paar ein oder updated dieses, falls dieses schon vorhanden sind
void insertHashmap(Hashmap& map, DWORD key, void* data)noexcept{
	DWORD idx = key % map.tableSize;
	HashmapData** current = &map.tableBuffer[idx];
	HashmapData* prev = *current;
	while(*current != nullptr){
		if((*current)->key == key){
			(*current)->data = data;
			return;
		}
		prev = *current;
		current = &((*current)->next);
	}
	*current = alloc<HashmapData>(1, "Hashmap-Entry");
	(*current)->data = data;
	(*current)->key = key;
	map.size++;
	if(prev) prev->next = *current;
}

//Löscht den Pointer aus der Hashmap und gibt den Zeiger auf die Daten zurück, nullptr falls nicht gefunden
void* removeHashmap(Hashmap& map, DWORD key)noexcept{
	DWORD idx = key % map.tableSize;
	HashmapData** current = &map.tableBuffer[idx];
	while(*current != nullptr){
		if((*current)->key == key){
			HashmapData* toDelete = (*current);
			*current = (*current)->next;
			void* data = toDelete->data;
			dealloc(toDelete);
			map.size--;
			return data;
		}
		current = &(*current)->next;
	}
	return nullptr;
}

void clearHashmap(Hashmap& map)noexcept{
	for(DWORD i=0; i < map.tableSize; ++i){
		HashmapData* current = map.tableBuffer[i];
		while(current != nullptr){
			HashmapData* toDelete = current;
			current = current->next;
			delete toDelete;
		}
		map.tableBuffer[i] = nullptr;
	}
	map.size = 0;
}

struct HashmapIterator{
	void* data = nullptr;				//Der Pointer in der Hashmap
	DWORD key;							//Der Key für den Pointer
	bool valid = false;					//True falls der Iterator valide ist, false sonst
	HashmapData* current = nullptr;		//Intern
	DWORD bucketIndex = 0;				//Der aktuelle "Bucket" Index
};

//Lässt den Iterator durch die Hashmap iterieren, falls der Iterator valid ist ist iterator.valid == true, sonst ist der Iterator invalide
HashmapIterator& iterateHashmap(Hashmap map, HashmapIterator& iterator)noexcept{
	while(1){
		if(iterator.bucketIndex >= map.tableSize){
			iterator.valid = false;
			break;
		}
		while(iterator.current != nullptr){
			iterator.data = iterator.current->data;
			iterator.key = iterator.current->key;
			iterator.current = iterator.current->next;
			iterator.valid = true;
			return iterator;
		}
		iterator.current = map.tableBuffer[iterator.bucketIndex];
		iterator.bucketIndex++;
	}
	return iterator;
}

void resetHashmapIterator(HashmapIterator& iterator){
	iterator.bucketIndex = 0;
	iterator.current = nullptr;
	iterator.data = nullptr;
}

//Befüllt das Array buffer mit allen Pointern die in der Hashmap sind
void getAllElementsHashmap(Hashmap& map, void** buffer)noexcept{
	DWORD idx = 0;
	for(DWORD i=0; i < map.tableSize; ++i){
		HashmapData* current = map.tableBuffer[i];
		while(current != nullptr){
			buffer[idx++] = current->data;
			current = current->next;
		}
	}
}

//Wie viele Pointer sich in der Hashmap befinden
DWORD sizeHashmap(Hashmap& map)noexcept{return map.size;}
