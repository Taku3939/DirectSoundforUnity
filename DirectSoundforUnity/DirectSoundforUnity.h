#pragma once
#define DLLEXPORT __declspec(dllexport)

extern "C" {
	//�Z�J���_���o�b�t�@�̍쐬
	DLLEXPORT int Load(const char* path);
}


extern "C" {
	//�Z�J���_���o�b�t�@�̍쐬
	DLLEXPORT BOOL Exit(void);
}

extern "C" {
	DLLEXPORT BOOL SetPosition(DWORD time);
}
extern "C" {
	DLLEXPORT BOOL Play();
}

extern "C" {
	DLLEXPORT BOOL Stop();
}

extern "C" {
	DLLEXPORT LPSTR GetSoundDevices();
}

extern "C" {
	DLLEXPORT long GetCurrentPosition();
}
extern "C" __declspec(dllexport) bool __stdcall GetText(byte buf, size_t bufsize) {
	if (bufsize < 4) { return false; }
		
	return true;
}
