#include "CWindow.h"
#include <dsound.h>


// ライブラリリンク
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

// グローバル変数
bool Loaded = false;

CWindow				win;					// ウインドウクラス
LPDIRECTSOUND8		lpDS = NULL;			// DirectSound8
LPDIRECTSOUNDBUFFER	lpPrimary = NULL;		// プライマリサウンドバッファ
LPDIRECTSOUNDBUFFER	lpSecondary = NULL;		// セカンダリサウンドバッファ

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
	GUID* pGUID,				              // デバイスのGUID
	LPSTR       strDesc,                        // デバイス名
	LPSTR       strDrvName,                     // ドライバー名
	VOID* pContext			                    // ユーザー引数
);

/////////////////////////////////////////////////////////
// 初期化
/////////////////////////////////////////////////////////
BOOL Init(void)
{
	HRESULT ret;
	
	// COMの初期化
	CoInitialize(NULL);

	// DirectSound8を作成
	ret = DirectSoundCreate8(NULL, &lpDS, NULL);
	if (FAILED(ret)) {
		return FALSE;
	}

	// 強調モード
	ret = lpDS->SetCooperativeLevel(win.hWnd, DSSCL_EXCLUSIVE | DSSCL_PRIORITY);
	if (FAILED(ret)) {
		return FALSE;
	}

	return TRUE;
}

/////////////////////////////////////////////////////////
// プライマリサウンドバッファの作成
/////////////////////////////////////////////////////////
BOOL CreatePrimaryBuffer(void)
{
	HRESULT ret;
	WAVEFORMATEX wf;

	// プライマリサウンドバッファの作成
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

	// プライマリバッファのステータスを決定
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
// サウンドバッファの作成
/////////////////////////////////////////////////////////
BOOL UpdateWave(LPDIRECTSOUNDBUFFER *dsb, const char* file)
{
	HRESULT ret;
	MMCKINFO mSrcWaveFile;
	MMCKINFO mSrcWaveFmt;
	MMCKINFO mSrcWaveData;
	LPWAVEFORMATEX wf;

	// WAVファイルをロード
	HMMIO hSrc = NULL;
	MMIOINFO mmioInfo;
	memset(&mmioInfo, 0, sizeof(MMIOINFO));
	hSrc = mmioOpenA((LPSTR)file, &mmioInfo, MMIO_ALLOCBUF | MMIO_READ | MMIO_COMPAT);
	if (!hSrc) {
		return FALSE;
	}

	// 'RIFE'チャンクチェック
	ZeroMemory(&mSrcWaveFile, sizeof(mSrcWaveFile));
	ret = mmioDescend(hSrc, &mSrcWaveFile, NULL, MMIO_FINDRIFF);
	if (mSrcWaveFile.fccType != mmioFOURCC('W', 'A', 'V', 'E')) {
		mmioClose(hSrc, 0);
		return FALSE;
	}

	// 'fmt 'チャンクチェック
	ZeroMemory(&mSrcWaveFmt, sizeof(mSrcWaveFmt));
	ret = mmioDescend(hSrc, &mSrcWaveFmt, &mSrcWaveFile, MMIO_FINDCHUNK);
	if (mSrcWaveFmt.ckid != mmioFOURCC('f', 'm', 't', ' ')) {
		mmioClose(hSrc, 0);
		return FALSE;
	}

	// ヘッダサイズの計算
	int iSrcHeaderSize = mSrcWaveFmt.cksize;
	if (iSrcHeaderSize < sizeof(WAVEFORMATEX))
		iSrcHeaderSize = sizeof(WAVEFORMATEX);

	// ヘッダメモリ確保
	wf = (LPWAVEFORMATEX)malloc(iSrcHeaderSize);
	if (!wf) {
		mmioClose(hSrc, 0);
		return FALSE;
	}
	ZeroMemory(wf, iSrcHeaderSize);

	// WAVEフォーマットのロード
	ret = mmioRead(hSrc, (char*)wf, mSrcWaveFmt.cksize);
	if (FAILED(ret)) {
		free(wf);
		mmioClose(hSrc, 0);
		return FALSE;
	}

	// fmtチャンクに戻る
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
	//	//音声データより大きなバッファを確保しようとした場合
	//	//バッファサイズを実際の音声のデータサイズにする
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
	//		//バッファコビー
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

	// dataチャンクを探す
	while (1) {
		// 検索
		//memset(&mSrcWaveData, 0, sizeof(MMCKINFO));
		ret = mmioDescend(hSrc, &mSrcWaveData, &mSrcWaveFile, 0);
		if (FAILED(ret)) {
			free(wf);
			mmioClose(hSrc, 0);
			return FALSE;
		}
		if (mSrcWaveData.ckid == mmioFOURCC('d', 'a', 't', 'a'))
			break;
		// 次のチャンクへ
		ret = mmioAscend(hSrc, &mSrcWaveData, 0);
	}


	// サウンドバッファの作成
	DSBUFFERDESC dsdesc;
	ZeroMemory(&dsdesc, sizeof(DSBUFFERDESC));
	dsdesc.dwSize = sizeof(DSBUFFERDESC);
	dsdesc.dwFlags = DSBCAPS_GETCURRENTPOSITION2	//再生位置の取得 
		| DSBCAPS_STATIC	 
		| DSBCAPS_LOCDEFER 
		| DSBCAPS_CTRLPOSITIONNOTIFY  // 再生位置通知イベントを有効にします
		| DSBCAPS_CTRLVOLUME          // ボリュームを変更可能にする
		| DSBCAPS_CTRLFREQUENCY       // 周波数を変更可能にする
		| DSBCAPS_CTRLPAN             // パンを変更可能にする
		| DSBCAPS_GLOBALFOCUS;	//バックグランドで動かすためのフラグ
	dsdesc.dwBufferBytes = mSrcWaveData.cksize;
	dsdesc.lpwfxFormat = wf;
	dsdesc.guid3DAlgorithm = DS3DALG_DEFAULT;
	ret = lpDS->CreateSoundBuffer(&dsdesc, dsb, NULL);
	if (FAILED(ret)) {
		free(wf);
		mmioClose(hSrc, 0);
		return FALSE;
	}
	
	// ロック開始
	LPVOID pMem1, pMem2;
	DWORD dwSize1, dwSize2;
	ret = (*dsb)->Lock(0, mSrcWaveData.cksize, &pMem1, &dwSize1, &pMem2, &dwSize2, 0);
	if (FAILED(ret)) {
		free(wf);
		mmioClose(hSrc, 0);
		return FALSE;
	}

	// データ書き込み
	mmioRead(hSrc, (char*)pMem1, dwSize1);
	mmioRead(hSrc, (char*)pMem2, dwSize2);

	// ロック解除
	(*dsb)->Unlock(pMem1, dwSize1, pMem2, dwSize2);

	// ヘッダ用メモリを開放
	free(wf);

	// WAVを閉じる
	mmioClose(hSrc, 0);

	return TRUE;
}

//BOOL WriteSoundBuffer(MMCKINFO* mSrcWaveData, ) {
//	// サウンドバッファの作成
//	DSBUFFERDESC dsdesc;
//	ZeroMemory(&dsdesc, sizeof(DSBUFFERDESC));
//	dsdesc.dwSize = sizeof(DSBUFFERDESC);
//	dsdesc.dwFlags = DSBCAPS_GETCURRENTPOSITION2	//再生位置の取得 
//		| DSBCAPS_STATIC
//		| DSBCAPS_LOCDEFER
//		| DSBCAPS_CTRLPOSITIONNOTIFY  // 再生位置通知イベントを有効にします
//		| DSBCAPS_CTRLVOLUME          // ボリュームを変更可能にする
//		| DSBCAPS_CTRLFREQUENCY       // 周波数を変更可能にする
//		| DSBCAPS_CTRLPAN             // パンを変更可能にする
//		| DSBCAPS_GLOBALFOCUS;	//バックグランドで動かすためのフラグ
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
//	// ロック開始
//	LPVOID pMem1, pMem2;
//	DWORD dwSize1, dwSize2;
//	ret = (*dsb)->Lock(0, mSrcWaveData.cksize, &pMem1, &dwSize1, &pMem2, &dwSize2, 0);
//	if (FAILED(ret)) {
//		free(wf);
//		mmioClose(hSrc, 0);
//		return FALSE;
//	}
//
//	// データ書き込み
//	mmioRead(hSrc, (char*)pMem1, dwSize1);
//	mmioRead(hSrc, (char*)pMem2, dwSize2);
//
//	// ロック解除
//	(*dsb)->Unlock(pMem1, dwSize1, pMem2, dwSize2);
//
//	// ヘッダ用メモリを開放
//	free(wf);
//}
//
//unsigned _stdcall ThereadUpdateData(void* pAguments, const char* fileName) {
//	//イベント通知を待つ
//	while (g_ThreadLoopFlag) {
//		if (WaitForSingleObject(g_Event[g_EventNum], 1000) == WAIT_OBJECT_0) {
//			LPVOID t_pWrite = NULL;
//			DWORD t_Length = 0;
//			if (lpSecondary->Lock(g_EventNum * UPDATE_BUF_SIZE, UPDATE_BUF_SIZE, &t_pWrite, &t_Length, NULL, NULL, 0) == DS_OK) {
//				char* t_pWaveData = NULL;
//				UpdateWave(fileName, &t_pWaveData, UPDATE_BUF_SIZE, g_OffSet);
//				if (t_pWaveData != NULL) {
//					//セカンダリバッファにコピー
//					memcpy(t_pWrite, &t_pWaveData[0], t_Length);
//					//セカンダリバッファにアンロック
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
// 終了
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

	// COMの終了
	CoUninitialize();
	Loaded = false;
	return TRUE;
}
BOOL WINAPI DllMain(HINSTANCE hinstDll, DWORD dwReason, LPVOID lpReserved)
{
	switch (dwReason) {

	case DLL_PROCESS_ATTACH: // DLLがプロセスのアドレス空間にマッピングされた。
		break;

	case DLL_THREAD_ATTACH: // スレッドが作成されようとしている。
		break;

	case DLL_THREAD_DETACH: // スレッドが破棄されようとしている。
		break;

	case DLL_PROCESS_DETACH: // DLLのマッピングが解除されようとしている。
		break;

	}

	// ウインドウ
	if (!win.Create(hinstDll, L"Window", FALSE)) {
		//DEBUG( "ウィンドウエラー\n" );
		return -1;
	}
	return TRUE;
}


// メインルーチン
int Load(const char* path)
{
	if (Loaded) {
		return 0;
	}

	Loaded = true;
	// DirectSound初期化
	if (!Init()) {
		Exit();
		return -1;
	}

	// プライマリサウンドバッファ
	if (!CreatePrimaryBuffer()) {
		Exit();
		return -1;
	}

	// サウンドバッファ
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
	//Play 第三引数にDSBPLAY_LOOPINGでloop
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
	GUID* pGUID,				              // デバイスのGUID
	LPSTR       strDesc,                        // デバイス名
	LPSTR       strDrvName,                     // ドライバー名
	VOID* pContext			                    // ユーザー引数
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