#include "CWindow.h"
#include <dsound.h>


// ���C�u���������N
#pragma comment(lib, "dsound.lib")
#pragma comment(lib, "winmm.lib")


//#pragma comment ( lib, "libogg.lib" )
//#pragma comment ( lib, "libvorbis_static.lib" )
//#pragma comment ( lib, "libvorbisfile_static.lib" )

#include "DirectSoundforUnity.h"
#include "CWindow.h"
#include "C:/Program Files (x86)/Microsoft DirectX SDK (June 2010)/Include/dsound.h"
#include <cstring>
#include <process.h>

// �O���[�o���ϐ�
bool Loaded = false;

CWindow				win;					// �E�C���h�E�N���X
LPDIRECTSOUND8		lpDS = NULL;			// DirectSound8
LPDIRECTSOUNDBUFFER	lpPrimary = NULL;		// �v���C�}���T�E���h�o�b�t�@
LPDIRECTSOUNDBUFFER	lpSecondary = NULL;		// �Z�J���_���T�E���h�o�b�t�@

const int NOTIFYEVENTS = 4;
const DWORD BUF_SIZE = 5000000;
const DWORD UPDATE_BUF_SIZE = (BUF_SIZE >> (NOTIFYEVENTS >> 1));
//IDirectSound8* g_pDS8;
//IDirectSoundBuffer8* g_pDSBuffer;

int g_ThreadLoopFlag = 1;
int g_EventNum = 0;
DWORD g_WaveSize = 0;
DWORD g_OffSet = 0;

HANDLE g_Event[NOTIFYEVENTS] = { NULL };

BOOL Exit(void);
int Load(const char* path);
BOOL SetPosition(DWORD time);
BOOL Play();
BOOL Stop();
LPSTR GetSoundDevices();
int CALLBACK DSEnumCallback(
	GUID* pGUID,				              // �f�o�C�X��GUID
	LPSTR       strDesc,                        // �f�o�C�X��
	LPSTR       strDrvName,                     // �h���C�o�[��
	VOID* pContext			                    // ���[�U�[����
);

/////////////////////////////////////////////////////////
// ������
/////////////////////////////////////////////////////////
BOOL Init(void)
{
	HRESULT ret;
	
	// COM�̏�����
	CoInitialize(NULL);

	// DirectSound8���쐬
	ret = DirectSoundCreate8(NULL, &lpDS, NULL);
	if (FAILED(ret)) {
		return FALSE;
	}

	// �������[�h
	ret = lpDS->SetCooperativeLevel(win.hWnd, DSSCL_EXCLUSIVE | DSSCL_PRIORITY);
	if (FAILED(ret)) {
		return FALSE;
	}

	return TRUE;
}

/////////////////////////////////////////////////////////
// �v���C�}���T�E���h�o�b�t�@�̍쐬
/////////////////////////////////////////////////////////
BOOL CreatePrimaryBuffer(void)
{
	HRESULT ret;
	WAVEFORMATEX wf;

	// �v���C�}���T�E���h�o�b�t�@�̍쐬
	DSBUFFERDESC dsdesc;
	ZeroMemory(&dsdesc, sizeof(DSBUFFERDESC));
	dsdesc.dwSize = sizeof(DSBUFFERDESC);
	dsdesc.dwFlags = DSBCAPS_PRIMARYBUFFER;
	dsdesc.dwBufferBytes = 0;
	dsdesc.lpwfxFormat = NULL;
	ret = lpDS->CreateSoundBuffer(&dsdesc, &lpPrimary, NULL);
	if (FAILED(ret)) {
		return FALSE;
	}

	// �v���C�}���o�b�t�@�̃X�e�[�^�X������
	wf.cbSize = sizeof(WAVEFORMATEX);
	wf.wFormatTag = WAVE_FORMAT_PCM;
	wf.nChannels = 2;
	wf.nSamplesPerSec = 44100;
	wf.wBitsPerSample = 16;
	wf.nBlockAlign = wf.nChannels * wf.wBitsPerSample / 8;
	wf.nAvgBytesPerSec = wf.nSamplesPerSec * wf.nBlockAlign;
	ret = lpPrimary->SetFormat(&wf);
	if (FAILED(ret)) {
		return FALSE;
	}

	return TRUE;
}


/////////////////////////////////////////////////////////
// �T�E���h�o�b�t�@�̍쐬
/////////////////////////////////////////////////////////
BOOL UpdateWave(LPDIRECTSOUNDBUFFER *dsb, const char* file)
{
	HRESULT ret;
	MMCKINFO mSrcWaveFile;
	MMCKINFO mSrcWaveFmt;
	MMCKINFO mSrcWaveData;
	LPWAVEFORMATEX wf;

	// WAV�t�@�C�������[�h
	HMMIO hSrc = NULL;
	MMIOINFO mmioInfo;
	memset(&mmioInfo, 0, sizeof(MMIOINFO));
	hSrc = mmioOpenA((LPSTR)file, &mmioInfo, MMIO_ALLOCBUF | MMIO_READ | MMIO_COMPAT);
	if (!hSrc) {
		return FALSE;
	}

	// 'RIFE'�`�����N�`�F�b�N
	ZeroMemory(&mSrcWaveFile, sizeof(mSrcWaveFile));
	ret = mmioDescend(hSrc, &mSrcWaveFile, NULL, MMIO_FINDRIFF);
	if (mSrcWaveFile.fccType != mmioFOURCC('W', 'A', 'V', 'E')) {
		mmioClose(hSrc, 0);
		return FALSE;
	}

	// 'fmt '�`�����N�`�F�b�N
	ZeroMemory(&mSrcWaveFmt, sizeof(mSrcWaveFmt));
	ret = mmioDescend(hSrc, &mSrcWaveFmt, &mSrcWaveFile, MMIO_FINDCHUNK);
	if (mSrcWaveFmt.ckid != mmioFOURCC('f', 'm', 't', ' ')) {
		mmioClose(hSrc, 0);
		return FALSE;
	}

	// �w�b�_�T�C�Y�̌v�Z
	int iSrcHeaderSize = mSrcWaveFmt.cksize;
	if (iSrcHeaderSize < sizeof(WAVEFORMATEX))
		iSrcHeaderSize = sizeof(WAVEFORMATEX);

	// �w�b�_�������m��
	wf = (LPWAVEFORMATEX)malloc(iSrcHeaderSize);
	if (!wf) {
		mmioClose(hSrc, 0);
		return FALSE;
	}
	ZeroMemory(wf, iSrcHeaderSize);

	// WAVE�t�H�[�}�b�g�̃��[�h
	ret = mmioRead(hSrc, (char*)wf, mSrcWaveFmt.cksize);
	if (FAILED(ret)) {
		free(wf);
		mmioClose(hSrc, 0);
		return FALSE;
	}

	// fmt�`�����N�ɖ߂�
	mmioAscend(hSrc, &mSrcWaveFmt, 0);

	/////------

	//mSrcWaveData.ckid = mmioFOURCC('d', 'a', 't', 'a');
	//ret = mmioDescend(hSrc, &mSrcWaveData, &mSrcWaveFile, MMIO_FINDCHUNK);
	//if (FAILED(ret)) {
	//	free(wf);
	//	mmioClose(hSrc, 0);
	//	return FALSE;
	//}
	//if (t_pReadSize != NULL) {
	//	//�����f�[�^���傫�ȃo�b�t�@���m�ۂ��悤�Ƃ����ꍇ
	//	//�o�b�t�@�T�C�Y�����ۂ̉����̃f�[�^�T�C�Y�ɂ���
	//	if (mSrcWaveData.cksize < t_BufSize) {
	//		*t_pReadSize = mSrcWaveData.cksize;
	//	}
	//	else {
	//		*t_pReadSize = t_BufSize;
	//	}
	//}

	//if (*t_ppData == NULL) {
	//	*t_ppData = new char[t_BufSize];
	//	if (mSrcWaveData.cksize > t_OffSet + t_BufSize) {
	//		DWORD t_Dif = t_OffSet % t_BufSize;
	//		mmioSeek(hSrc, t_OffSet, SEEK_SET);
	//		size = mmioRead(hSrc, (HPSTR)(*t_ppData), t_BufSize);
	//		t_OffSet += t_BufSize;
	//	}
	//	else {
	//		DWORD t_BufSizeBegin = mSrcWaveData.cksize - t_OffSet;
	//		DWORD t_BufSizeEnd = t_BufSize - (mSrcWaveData.cksize - t_OffSet);
	//		DWORD t_Dif = t_OffSet % t_BufSize;
	//		char* t_BufBegin = new char[t_BufSizeBegin];
	//		char* t_BufEnd = new char[t_BufSizeEnd];
	//		size = mmioRead(hSrc, (HPSTR)t_BufEnd, t_BufSizeEnd);
	//		mmioSeek(hSrc, t_BufSizeBegin, SEEK_END);
	//		size = mmioRead(hSrc, (HPSTR)t_BufBegin, t_BufSizeBegin);
	//		//�o�b�t�@�R�r�[
	//		for (DWORD i = 0; i < t_BufSizeEnd; i++) {
	//			*(*t_ppData + i) = t_BufEnd[i];
	//		}
	//		for (DWORD i = 0; i < t_BufSizeBegin; i++) {
	//			*(*t_ppData + t_BufSizeEnd + i) = t_BufBegin[i];
	//		}
	//		t_OffSet = t_BufSizeEnd;
	//		delete[] t_BufBegin;
	//		delete[] t_BufEnd;
	//	}
	//}

	//------

	// data�`�����N��T��
	while (1) {
		// ����
		//memset(&mSrcWaveData, 0, sizeof(MMCKINFO));
		ret = mmioDescend(hSrc, &mSrcWaveData, &mSrcWaveFile, 0);
		if (FAILED(ret)) {
			free(wf);
			mmioClose(hSrc, 0);
			return FALSE;
		}
		if (mSrcWaveData.ckid == mmioFOURCC('d', 'a', 't', 'a'))
			break;
		// ���̃`�����N��
		ret = mmioAscend(hSrc, &mSrcWaveData, 0);
	}


	// �T�E���h�o�b�t�@�̍쐬
	DSBUFFERDESC dsdesc;
	ZeroMemory(&dsdesc, sizeof(DSBUFFERDESC));
	dsdesc.dwSize = sizeof(DSBUFFERDESC);
	dsdesc.dwFlags = DSBCAPS_GETCURRENTPOSITION2	//�Đ��ʒu�̎擾 
		| DSBCAPS_STATIC	 
		| DSBCAPS_LOCDEFER 
		| DSBCAPS_CTRLPOSITIONNOTIFY  // �Đ��ʒu�ʒm�C�x���g��L���ɂ��܂�
		| DSBCAPS_CTRLVOLUME          // �{�����[����ύX�\�ɂ���
		| DSBCAPS_CTRLFREQUENCY       // ���g����ύX�\�ɂ���
		| DSBCAPS_CTRLPAN             // �p����ύX�\�ɂ���
		| DSBCAPS_GLOBALFOCUS;	//�o�b�N�O�����h�œ��������߂̃t���O
	dsdesc.dwBufferBytes = mSrcWaveData.cksize;
	dsdesc.lpwfxFormat = wf;
	dsdesc.guid3DAlgorithm = DS3DALG_DEFAULT;
	ret = lpDS->CreateSoundBuffer(&dsdesc, dsb, NULL);
	if (FAILED(ret)) {
		free(wf);
		mmioClose(hSrc, 0);
		return FALSE;
	}
	
	// ���b�N�J�n
	LPVOID pMem1, pMem2;
	DWORD dwSize1, dwSize2;
	ret = (*dsb)->Lock(0, mSrcWaveData.cksize, &pMem1, &dwSize1, &pMem2, &dwSize2, 0);
	if (FAILED(ret)) {
		free(wf);
		mmioClose(hSrc, 0);
		return FALSE;
	}

	// �f�[�^��������
	mmioRead(hSrc, (char*)pMem1, dwSize1);
	mmioRead(hSrc, (char*)pMem2, dwSize2);

	// ���b�N����
	(*dsb)->Unlock(pMem1, dwSize1, pMem2, dwSize2);

	// �w�b�_�p���������J��
	free(wf);

	// WAV�����
	mmioClose(hSrc, 0);

	return TRUE;
}

//BOOL WriteSoundBuffer(MMCKINFO* mSrcWaveData, ) {
//	// �T�E���h�o�b�t�@�̍쐬
//	DSBUFFERDESC dsdesc;
//	ZeroMemory(&dsdesc, sizeof(DSBUFFERDESC));
//	dsdesc.dwSize = sizeof(DSBUFFERDESC);
//	dsdesc.dwFlags = DSBCAPS_GETCURRENTPOSITION2	//�Đ��ʒu�̎擾 
//		| DSBCAPS_STATIC
//		| DSBCAPS_LOCDEFER
//		| DSBCAPS_CTRLPOSITIONNOTIFY  // �Đ��ʒu�ʒm�C�x���g��L���ɂ��܂�
//		| DSBCAPS_CTRLVOLUME          // �{�����[����ύX�\�ɂ���
//		| DSBCAPS_CTRLFREQUENCY       // ���g����ύX�\�ɂ���
//		| DSBCAPS_CTRLPAN             // �p����ύX�\�ɂ���
//		| DSBCAPS_GLOBALFOCUS;	//�o�b�N�O�����h�œ��������߂̃t���O
//	dsdesc.dwBufferBytes = mSrcWaveData.cksize;
//	dsdesc.lpwfxFormat = wf;
//	dsdesc.guid3DAlgorithm = DS3DALG_DEFAULT;
//	ret = lpDS->CreateSoundBuffer(&dsdesc, dsb, NULL);
//	if (FAILED(ret)) {
//		free(wf);
//		mmioClose(hSrc, 0);
//		return FALSE;
//	}
//
//	// ���b�N�J�n
//	LPVOID pMem1, pMem2;
//	DWORD dwSize1, dwSize2;
//	ret = (*dsb)->Lock(0, mSrcWaveData.cksize, &pMem1, &dwSize1, &pMem2, &dwSize2, 0);
//	if (FAILED(ret)) {
//		free(wf);
//		mmioClose(hSrc, 0);
//		return FALSE;
//	}
//
//	// �f�[�^��������
//	mmioRead(hSrc, (char*)pMem1, dwSize1);
//	mmioRead(hSrc, (char*)pMem2, dwSize2);
//
//	// ���b�N����
//	(*dsb)->Unlock(pMem1, dwSize1, pMem2, dwSize2);
//
//	// �w�b�_�p���������J��
//	free(wf);
//}
//
//unsigned _stdcall ThereadUpdateData(void* pAguments, const char* fileName) {
//	//�C�x���g�ʒm��҂�
//	while (g_ThreadLoopFlag) {
//		if (WaitForSingleObject(g_Event[g_EventNum], 1000) == WAIT_OBJECT_0) {
//			LPVOID t_pWrite = NULL;
//			DWORD t_Length = 0;
//			if (lpSecondary->Lock(g_EventNum * UPDATE_BUF_SIZE, UPDATE_BUF_SIZE, &t_pWrite, &t_Length, NULL, NULL, 0) == DS_OK) {
//				char* t_pWaveData = NULL;
//				UpdateWave(fileName, &t_pWaveData, UPDATE_BUF_SIZE, g_OffSet);
//				if (t_pWaveData != NULL) {
//					//�Z�J���_���o�b�t�@�ɃR�s�[
//					memcpy(t_pWrite, &t_pWaveData[0], t_Length);
//					//�Z�J���_���o�b�t�@�ɃA�����b�N
//					lpSecondary->Unlock(t_pWrite, t_Length, NULL, 0);
//
//					delete[] t_pWaveData;
//
//					g_EventNum = (g_EventNum + 1) % NOTIFYEVENTS;
//				}
//			}
//		}
//	}
//	_endthreadex(0);
//	return 0;
//}


/////////////////////////////////////////////////////////
// �I��
/////////////////////////////////////////////////////////
BOOL Exit(void)
{
	if (lpSecondary) {
		lpSecondary->Release();
		lpSecondary = NULL;
	}

	if (lpPrimary) {
		lpPrimary->Release();
		lpPrimary = NULL;
	}

	if (lpDS) {
		lpDS->Release();
		lpDS = NULL;
	}

	// COM�̏I��
	CoUninitialize();
	Loaded = false;
	return TRUE;
}
BOOL WINAPI DllMain(HINSTANCE hinstDll, DWORD dwReason, LPVOID lpReserved)
{
	switch (dwReason) {

	case DLL_PROCESS_ATTACH: // DLL���v���Z�X�̃A�h���X��ԂɃ}�b�s���O���ꂽ�B
		break;

	case DLL_THREAD_ATTACH: // �X���b�h���쐬����悤�Ƃ��Ă���B
		break;

	case DLL_THREAD_DETACH: // �X���b�h���j������悤�Ƃ��Ă���B
		break;

	case DLL_PROCESS_DETACH: // DLL�̃}�b�s���O����������悤�Ƃ��Ă���B
		break;

	}

	// �E�C���h�E
	if (!win.Create(hinstDll, L"Window", FALSE)) {
		//DEBUG( "�E�B���h�E�G���[\n" );
		return -1;
	}
	return TRUE;
}


// ���C�����[�`��
int Load(const char* path)
{
	if (Loaded) {
		return 0;
	}

	Loaded = true;
	// DirectSound������
	if (!Init()) {
		Exit();
		return -1;
	}

	// �v���C�}���T�E���h�o�b�t�@
	if (!CreatePrimaryBuffer()) {
		Exit();
		return -1;
	}

	// �T�E���h�o�b�t�@
	if (!UpdateWave(&lpSecondary, path)){
			Exit();
			return -1;
	}

	return 0;
}
DWORD       m_dwLastPlayCursor;

enum tPlayState
{
	StateStopped,
	StatePaused,
	StateRunning
} PLAYSTATE;



BOOL SetPosition(DWORD pos) {
	lpSecondary = NULL;

	if (Loaded) {
		/*if ( != DS_OK) {
			return -1;


		}*/
	}
	return 0;
}

BOOL Play() {
	//Play ��O������DSBPLAY_LOOPING��loop
	if (Loaded) {
		if (lpSecondary->Play(0, 0, 0) != DS_OK) {
			return -1;
		}
	}
	return 0;
}

BOOL Stop() {
	if (Loaded) {
		if (lpSecondary->Stop() != DS_OK) {
			return -1;
		}
	}
	return 0;
}

LPSTR device;
LPSTR GetSoundDevices() {
	DirectSoundEnumerate((LPDSENUMCALLBACK)DSEnumCallback, (void*)0);
	return device;
}

int CALLBACK DSEnumCallback(
	GUID* pGUID,				              // �f�o�C�X��GUID
	LPSTR       strDesc,                        // �f�o�C�X��
	LPSTR       strDrvName,                     // �h���C�o�[��
	VOID* pContext			                    // ���[�U�[����
) {
	device = strDesc;
	return 0;
}

BOOL SetVolume(LONG volume) {
	if (Loaded) {
		if (lpSecondary->SetVolume(volume) != DS_OK) {
			return -1;
		}
	}
	return 0;
}


BOOL SetPan(LONG pan) {
	if (Loaded) {
		if (lpSecondary->SetPan(pan) != DS_OK) {
			return -1;
		}
	}
	return 0;
}
LPDWORD pwdplay;
long GetCurrentPosition();
long GetCurrentPosition() {
	if (Loaded) {
		if (lpSecondary->GetCurrentPosition(pwdplay, NULL) != DS_OK) {
			return -1;
		}
		
	}
	return (long)pwdplay;
}