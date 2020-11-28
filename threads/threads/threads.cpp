// FFmpegX264Codec.cpp : Defines the entry point for the application.
//


#include "threads.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <new>
#include "ScreenCapture.h"
#include <d3d9.h>
#include <thread>
#include <mutex>
#include <tchar.h>
#include <strsafe.h>
using namespace std;
#define MAX_LOADSTRING 100
std::mutex m1;
CRITICAL_SECTION  m_critial;

IDirect3D9* m_pDirect3D9 = NULL;
IDirect3DDevice9* m_pDirect3DDevice = NULL;
IDirect3DSurface9* m_pDirect3DSurfaceRender = NULL;

RECT m_rtViewport;
RECT desktop;


#define INBUF_SIZE 20480

#define MAX_LOADSTRING 100

UCHAR * CaptureBuffer = NULL;

// Global Variables:
HINSTANCE hInst;                        // current instance
TCHAR szTitle[MAX_LOADSTRING];          // The title bar text
TCHAR szWindowClass[MAX_LOADSTRING];    // the main window class name

// Forward declarations of functions included in this code module:

ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);


bool captured = false;
AVPacket* pkt, * pkt1, * pkt2, * pkt3, * pkt4, * pkt5;
std::mutex mtx, mtx1, mtx2, mtx3;
//std::thread th1, th2;

typedef struct MyData {
	AVFrame* frame;
	AVPacket* p;
	AVCodecContext* av;
	FILE* file;
} MYDATA, * PMYDATA;


void Cleanup()
{
	EnterCriticalSection(&m_critial);
	if (m_pDirect3DSurfaceRender)
		m_pDirect3DSurfaceRender->Release();
	if (m_pDirect3DDevice)
		m_pDirect3DDevice->Release();
	if (m_pDirect3D9)
		m_pDirect3D9->Release();
	LeaveCriticalSection(&m_critial);
}

void AVCodecCleanup()
{
	free(is_keyframe);

	fclose(log_file);
	free(CaptureBuffer);

	avcodec_close(encodingCodecContext);
	avcodec_close(decodingCodecContext);
	avcodec_free_context(&decodingCodecContext);
	av_frame_free(&iframe);
	av_frame_free(&oframe);
	avcodec_free_context(&encodingCodecContext);
	av_frame_free(&inframe);
	av_frame_free(&outframe);
	av_packet_free(&pkt);
	av_packet_free(&pkt1);
	Cleanup();

	exit(0);
}

int InitD3D(HWND hwnd, unsigned long lWidth, unsigned long lHeight)
{
	HRESULT lRet;
	InitializeCriticalSection(&m_critial);
	Cleanup();

	m_pDirect3D9 = Direct3DCreate9(D3D_SDK_VERSION);
	if (m_pDirect3D9 == NULL)
		return -1;

	D3DPRESENT_PARAMETERS d3dpp;
	ZeroMemory(&d3dpp, sizeof(d3dpp));
	d3dpp.Windowed = TRUE;
	d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
	d3dpp.BackBufferFormat = D3DFMT_UNKNOWN;

	GetClientRect(hwnd, &m_rtViewport);

	lRet = m_pDirect3D9->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hwnd,
		D3DCREATE_HARDWARE_VERTEXPROCESSING,
		&d3dpp, &m_pDirect3DDevice);
	if (FAILED(lRet))
		return -1;

	lRet = m_pDirect3DDevice->CreateOffscreenPlainSurface(
		lWidth, lHeight,
		(D3DFORMAT)MAKEFOURCC('Y', 'V', '1', '2'), //FourCC for YUV420P Pixal Color Format
		D3DPOOL_DEFAULT,
		&m_pDirect3DSurfaceRender,
		NULL);


	if (FAILED(lRet))
		return -1;

	return 0;
}




int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPWSTR    lpCmdLine,
	_In_ int       nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

	// TODO: Place code here.

	// Initialize global strings
	LoadString(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
	LoadString(hInstance, IDC_THREADS, szWindowClass, MAX_LOADSTRING);
	MyRegisterClass(hInstance);

	// Perform application initialization:
	if (!InitInstance(hInstance, nCmdShow))
	{
		return FALSE;
	}

	HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_THREADS));

	MSG msg;

	// Main message loop:
	// Main message loop:

	ZeroMemory(&msg, sizeof(msg));
	driver(msg.hwnd);
	AVCodecCleanup();

	UnregisterClass(szWindowClass, hInstance);
	return 0;
}



//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
	WNDCLASSEX wcex;

	wcex.cbSize = sizeof(WNDCLASSEX);

	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = WndProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = hInstance;
	wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDC_THREADS));
	wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wcex.lpszMenuName = MAKEINTRESOURCE(IDC_THREADS);
	wcex.lpszClassName = szWindowClass;
	wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

	return RegisterClassEx(&wcex);


}

//
//   FUNCTION: InitInstance(HINSTANCE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create and display the main program window.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
	HWND hWnd;

	hInst = hInstance; // Store instance handle in our global variable

	int pixel_w = 1008, pixel_h = 465;

	hWnd = CreateWindow(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW, 0, 0, pixel_w, pixel_h, NULL, NULL, hInstance, NULL);

	if (!hWnd)
	{
		return FALSE;
	}

	GetWindowRect(GetDesktopWindow(), &desktop);
	pixel_w = desktop.right - desktop.left;
	pixel_h = desktop.bottom - desktop.top;

	if (InitD3D(hWnd, pixel_w, pixel_h) == E_FAIL) {
		return FALSE;
	}
	return TRUE;
}



DWORD WINAPI encode(LPVOID lpParam)
{
	int ret;
	HANDLE hStdout;
	PMYDATA pDataArray;
	pDataArray = (PMYDATA)lpParam;
	hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
	if (hStdout == INVALID_HANDLE_VALUE)
		return 1;
	//std::thread::id threadID = std::this_thread::get_id();
	//fprintf(log_file, "\nthread_id:%d\n", std::this_thread::get_id());
	/* send the frame to the encoder */
	if (pDataArray->frame)
		printf("Send frame %d\n", pDataArray->frame->pts);

	ret = avcodec_send_frame(pDataArray->av, pDataArray->frame);// send frame to encode
	//fprintf(log_file, "\nthread_id:%d ---- encoded---\n", std::this_thread::get_id());

	if (ret < 0) {
		fprintf(log_file, "Error sending a frame for encoding\n");
		AVCodecCleanup();
		return NULL;
	}

	while (ret >= 0)
	{
		ret = avcodec_receive_packet(pDataArray->av, pDataArray->p);// read encoded data and saved in pkt
		//fprintf(log_file, "\nthread_id:%d ---- read encoded data ---\n", std::this_thread::get_id());
		if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
			return NULL;
		}

		else if (ret < 0) {
			fprintf(log_file, "Error during encoding\n");
			AVCodecCleanup();
		}
		start_decode = clock();
		stop_decode = clock();
		nDiff_decode = stop_decode - start_decode;
		time_for_decode += nDiff_decode;
		number_of_packets_decoded++;
		//fprintf(log_file, "time for decode - %d\n", nDiff);

		//fprintf(log_file, "size of packet- %d\n", pkt->size);
		total_size_of_packets += pDataArray->p->size;
		number_of_packets++;
		fwrite(pDataArray->p->data, 1, pDataArray->p->size, pDataArray->file);
		av_packet_unref(pDataArray->p);


	}
	return NULL;
}


int width = 0, height = 0;  //initial values
int ret;
void contextinit(const AVCodec* codec, AVCodecContext** ecc, AVPacket** p, const char* i, const char** filename, FILE **file) {
	
	
	*ecc = avcodec_alloc_context3(codec);
	if (!*ecc) {
		fprintf(log_file, "Could not allocate video codec context\n");
		AVCodecCleanup();
	}
	*p = av_packet_alloc();// after encoding save in this
	if (!*p)
		AVCodecCleanup();
	(*ecc)->bit_rate = 4000000;
	/* resolution must be a multiple of two */
	// Get the client area for size calculation
	RECT rcClient;
	GetClientRect(NULL, &rcClient);
	const HWND hDesktop = GetDesktopWindow();

	// Get the size of screen to the variable desktop
	GetWindowRect(hDesktop, &desktop);

	// The top left corner will have coordinates (0,0)
	// and the bottom right corner will have coordinates
	// (horizontal, vertical)
	width = desktop.right;
	height = desktop.bottom;

	// We always want the input to encode as 
	// a frame with a fixed resolution 
	// irrespective of screen resolution.
	(*ecc)->width = width;
	(*ecc)->height = height;

	/* frames per second */
	(*ecc)->time_base.num = 1;
	(*ecc)->time_base.den = 60;
	(*ecc)->framerate.num = 60;
	(*ecc)->framerate.den = 1;


	(*ecc)->pix_fmt = AV_PIX_FMT_YUV420P;


	if (codec->id == AV_CODEC_ID_H264)
	{
		av_opt_set((*ecc)->priv_data, "preset", "veryfast", 0);
		av_opt_set((*ecc)->priv_data, "tune", "zerolatency", 0);
		av_opt_set((*ecc)->priv_data, "crf", i, 0);

	}

	/* open it */
	
	ret = avcodec_open2((*ecc), codec, NULL);
	if (ret < 0) {
		printf("Could not open codec: \n");
		AVCodecCleanup();
	}
	fopen_s(file, *filename, "wb");
	if (!file) {
		fprintf(log_file, "Could not open %s\n", *filename);
		AVCodecCleanup();
	}

}

HANDLE  hThreadArray[6];
void driver(HWND hWnd)
{
	const char* filename, * codec_name, * folder_name, * filename1, * filename2, * filename3, * filename4, * filename5;
	const AVCodec* codec;

	uint8_t endcode[] = { 0, 0, 1, 0xb7 };//footer

	fopen_s(&log_file, "log.txt", "w");

	filename = "Content\\encoded.h264";
	filename1 = "Content\\encoded1.h264";
	filename2 = "Content\\encoded2.h264";
	filename3 = "Content\\encoded3.h264";
	filename4 = "Content\\encoded4.h264";
	filename5 = "Content\\encoded5.h264";

	avcodec_register_all();

	/* find the video encoder */
	codec = avcodec_find_encoder_by_name("libx264");
	if (!codec) {
		fprintf(log_file, "Codec libx264 not found\n");
		AVCodecCleanup();
	}
	contextinit(codec, &encodingCodecContext, &pkt, "51", &filename, &h264_file);
	contextinit(codec, &encodingCodecContext1, &pkt1, "45", &filename1, &h264_file1);
	contextinit(codec, &encodingCodecContext2, &pkt2, "37", &filename2, &h264_file2);
	contextinit(codec, &encodingCodecContext3, &pkt3, "30", &filename3, &h264_file3);
	contextinit(codec, &encodingCodecContext4, &pkt4, "22", &filename4, &h264_file4);
	contextinit(codec, &encodingCodecContext5, &pkt5, "18", &filename5, &h264_file5);
	
	//pkt1 = av_packet_alloc();// after encoding save in this
	//if (!pkt1)
	//	AVCodecCleanup();
	//pkt2 = av_packet_alloc();// after encoding save in this
	//if (!pkt2)
	//	AVCodecCleanup();
	//pkt3 = av_packet_alloc();// after encoding save in this
	//if (!pkt3)
	//	AVCodecCleanup();
	//pkt4 = av_packet_alloc();// after encoding save in this
	//if (!pkt4)
	//	AVCodecCleanup();
	//pkt5 = av_packet_alloc();// after encoding save in this
	//if (!pkt5)
	//	AVCodecCleanup();
	
	//if (codec->id == AV_CODEC_ID_H264)
	//{
	//	av_opt_set(encodingCodecContext->priv_data, "preset", "veryfast", 0);
	//	av_opt_set(encodingCodecContext->priv_data, "tune", "zerolatency", 0);
	//	av_opt_set(encodingCodecContext->priv_data, "crf", "51", 0);

	//}
	//if (codec1->id == AV_CODEC_ID_H264)
	//{
	//	av_opt_set(encodingCodecContext1->priv_data, "preset", "veryfast", 0);
	//	av_opt_set(encodingCodecContext1->priv_data, "tune", "zerolatency", 0);
	//	av_opt_set(encodingCodecContext1->priv_data, "crf", "44", 0);

	//}
	//if (codec2->id == AV_CODEC_ID_H264)
	//{
	//	av_opt_set(encodingCodecContext2->priv_data, "preset", "veryfast", 0);
	//	av_opt_set(encodingCodecContext2->priv_data, "tune", "zerolatency", 0);
	//	av_opt_set(encodingCodecContext2->priv_data, "crf", "37", 0);

	//}

	//if (codec3->id == AV_CODEC_ID_H264)
	//{
	//	av_opt_set(encodingCodecContext3->priv_data, "preset", "veryfast", 0);
	//	av_opt_set(encodingCodecContext3->priv_data, "tune", "zerolatency", 0);
	//	av_opt_set(encodingCodecContext3->priv_data, "crf", "30", 0);

	//}
	//if (codec4->id == AV_CODEC_ID_H264)
	//{
	//	av_opt_set(encodingCodecContext4->priv_data, "preset", "veryfast", 0);
	//	av_opt_set(encodingCodecContext4->priv_data, "tune", "zerolatency", 0);
	//	av_opt_set(encodingCodecContext4->priv_data, "crf", "25", 0);

	//}
	//if (codec5->id == AV_CODEC_ID_H264)
	//{
	//	av_opt_set(encodingCodecContext5->priv_data, "preset", "veryfast", 0);
	//	av_opt_set(encodingCodecContext5->priv_data, "tune", "zerolatency", 0);
	//	av_opt_set(encodingCodecContext5->priv_data, "crf", "18", 0);

	//}
	///* put sample parameters */
	//encodingCodecContext->bit_rate = 4000000;
	//encodingCodecContext1->bit_rate = 4000000;
	//encodingCodecContext2->bit_rate = 4000000;
	//encodingCodecContext3->bit_rate = 4000000;
	//encodingCodecContext4->bit_rate = 4000000;
	//encodingCodecContext5->bit_rate = 4000000;

	///* resolution must be a multiple of two */
	//// Get the client area for size calculation
	//RECT rcClient;
	//GetClientRect(NULL, &rcClient);

	//int width = 0, height = 0;  //initial values


	//// Get a handle to the desktop window
	//const HWND hDesktop = GetDesktopWindow();

	//// Get the size of screen to the variable desktop
	//GetWindowRect(hDesktop, &desktop);

	//// The top left corner will have coordinates (0,0)
	//// and the bottom right corner will have coordinates
	//// (horizontal, vertical)
	//width = desktop.right;
	//height = desktop.bottom;

	//// We always want the input to encode as
	//// a frame with a fixed resolution
	//// irrespective of screen resolution.
	//encodingCodecContext->width = width;
	//encodingCodecContext->height = height;
	//encodingCodecContext1->width = width;
	//encodingCodecContext1->height = height;
	//encodingCodecContext2->width = width;
	//encodingCodecContext2->height = height;
	//encodingCodecContext3->width = width;
	//encodingCodecContext3->height = height;
	//encodingCodecContext4->width = width;
	//encodingCodecContext4->height = height;
	//encodingCodecContext5->width = width;
	//encodingCodecContext5->height = height;

	///* frames per second */
	//encodingCodecContext->time_base.num = 1;
	//encodingCodecContext->time_base.den = 60;
	//encodingCodecContext->framerate.num = 60;
	//encodingCodecContext->framerate.den = 1;
	//encodingCodecContext1->time_base.num = 1;
	//encodingCodecContext1->time_base.den = 60;
	//encodingCodecContext1->framerate.num = 60;
	//encodingCodecContext1->framerate.den = 1;
	//encodingCodecContext2->time_base.num = 1;
	//encodingCodecContext2->time_base.den = 60;
	//encodingCodecContext2->framerate.num = 60;
	//encodingCodecContext2->framerate.den = 1;
	//encodingCodecContext3->time_base.num = 1;
	//encodingCodecContext3->time_base.den = 60;
	//encodingCodecContext3->framerate.num = 60;
	//encodingCodecContext3->framerate.den = 1;
	//encodingCodecContext4->time_base.num = 1;
	//encodingCodecContext4->time_base.den = 60;
	//encodingCodecContext4->framerate.num = 60;
	//encodingCodecContext4->framerate.den = 1;
	//encodingCodecContext5->time_base.num = 1;
	//encodingCodecContext5->time_base.den = 60;
	//encodingCodecContext5->framerate.num = 60;
	//encodingCodecContext5->framerate.den = 1;


	//encodingCodecContext->pix_fmt = AV_PIX_FMT_YUV420P;
	//encodingCodecContext1->pix_fmt = AV_PIX_FMT_YUV420P;
	//encodingCodecContext2->pix_fmt = AV_PIX_FMT_YUV420P;
	//encodingCodecContext3->pix_fmt = AV_PIX_FMT_YUV420P;
	//encodingCodecContext4->pix_fmt = AV_PIX_FMT_YUV420P;
	//encodingCodecContext5->pix_fmt = AV_PIX_FMT_YUV420P;


	//
	///* open it */
	//int ret;
	//ret = avcodec_open2(encodingCodecContext, codec, NULL);
	//if (ret < 0) {
	//	printf("Could not open codec: \n");
	//	AVCodecCleanup();
	//}
	//ret = avcodec_open2(encodingCodecContext1, codec1, NULL);
	//if (ret < 0) {
	//	printf("Could not open codec1: \n");
	//	AVCodecCleanup();
	//}
	//ret = avcodec_open2(encodingCodecContext2, codec, NULL);
	//if (ret < 0) {
	//	printf("Could not open codec: \n");
	//	AVCodecCleanup();
	//}
	//ret = avcodec_open2(encodingCodecContext3, codec1, NULL);
	//if (ret < 0) {
	//	printf("Could not open codec1: \n");
	//	AVCodecCleanup();
	//}
	//ret = avcodec_open2(encodingCodecContext4, codec, NULL);
	//if (ret < 0) {
	//	printf("Could not open codec: \n");
	//	AVCodecCleanup();
	//}
	//ret = avcodec_open2(encodingCodecContext5, codec1, NULL);
	//if (ret < 0) {
	//	printf("Could not open codec1: \n");
	//	AVCodecCleanup();
	//}

	

	/*fopen_s(&h264_file1, filename1, "wb");
	if (!h264_file1) {
		fprintf(log_file, "Could not open %s\n", filename1);
		AVCodecCleanup();
	}
	fopen_s(&h264_file2, filename2, "wb");
	if (!h264_file2) {
		fprintf(log_file, "Could not open %s\n", filename2);
		AVCodecCleanup();
	}

	fopen_s(&h264_file3, filename3, "wb");
	if (!h264_file3) {
		fprintf(log_file, "Could not open %s\n", filename3);
		AVCodecCleanup();
	}
	fopen_s(&h264_file4, filename4, "wb");
	if (!h264_file4) {
		fprintf(log_file, "Could not open %s\n", filename4);
		AVCodecCleanup();
	}

	fopen_s(&h264_file5, filename5, "wb");
	if (!h264_file5) {
		fprintf(log_file, "Could not open %s\n", filename5);
		AVCodecCleanup();
	}*/


	inframe = av_frame_alloc();
	if (!inframe) {
		fprintf(log_file, "Could not allocate video frame\n");
		AVCodecCleanup();
	}
	inframe->format = AV_PIX_FMT_RGB32;
	inframe->width = width;
	inframe->height = height;

	ret = av_frame_get_buffer(inframe, 32);
	if (ret < 0) {
		fprintf(log_file, "Could not allocate the video frame data\n");
		AVCodecCleanup();
	}


	outframe = av_frame_alloc();
	if (!outframe) {
		fprintf(log_file, "Could not allocate video frame\n");
		AVCodecCleanup();
	}
	outframe->format = AV_PIX_FMT_YUV420P;
	outframe->width = encodingCodecContext->width;
	outframe->height = encodingCodecContext->height;



	ret = av_frame_get_buffer(outframe, 32);
	if (ret < 0) {
		fprintf(log_file, "Could not allocate the video frame data\n");
		AVCodecCleanup();
	}


	iframe = av_frame_alloc();
	if (!iframe) {
		fprintf(log_file, "Could not allocate video frame\n");
		AVCodecCleanup();
	}

	iframe->format = AV_PIX_FMT_YUV420P;
	iframe->width = encodingCodecContext->width;
	iframe->height = encodingCodecContext->height;

	ret = av_frame_get_buffer(iframe, 32);
	if (ret < 0) {
		fprintf(log_file, "Could not allocate the video frame data\n");
		AVCodecCleanup();
	}

	long CaptureLineSize = NULL;
	ScreenCaptureProcessorGDI* D3D11 = new ScreenCaptureProcessorGDI();

	//Capture part ends here.

	D3D11->init();


	CaptureBuffer = (UCHAR*)malloc(inframe->height * inframe->linesize[0]);

	rgb_to_yuv_SwsContext = sws_getContext(inframe->width, inframe->height, AV_PIX_FMT_RGB32,
		outframe->width, outframe->height, AV_PIX_FMT_YUV420P,
		SWS_BICUBIC, NULL, NULL, NULL);
	/* create scaling context */

	if (!rgb_to_yuv_SwsContext) {
		fprintf(log_file,
			"Impossible to create scale context for the conversion "
			"fmt:%s s:%dx%d -> fmt:%s s:%dx%d\n",
			av_get_pix_fmt_name(AV_PIX_FMT_RGB32), inframe->width, inframe->height,
			av_get_pix_fmt_name(AV_PIX_FMT_YUV420P), outframe->width, outframe->height);
		ret = AVERROR(EINVAL);
		AVCodecCleanup();
	}

	char buf[1024];

	int number_of_frames_to_process = 100;
	is_keyframe = (char*)malloc(number_of_frames_to_process + 10);
	PMYDATA pDataArray[6];
	DWORD   dwThreadIdArray[6];
	

	for (int i = 0; i < number_of_frames_to_process; i++)
	{
		fflush(stdout);
		char* srcData = NULL;
		start = clock();
		//m1.lock();
		D3D11->GrabImage(CaptureBuffer, CaptureLineSize);
		//m1.unlock();
		stop = clock();
		nDiff = stop - start;
		time_for_capture += nDiff;
		number_of_frames_captured++;

		//fprintf(file, "time for capture - %d\n", nDiff);

		uint8_t* inData[1] = { (uint8_t*)CaptureBuffer }; // RGB have one plane
		int inLinesize[1] = { av_image_get_linesize(AV_PIX_FMT_RGB32, inframe->width, 0) }; // RGB stride
		start = clock();
		m1.lock();
		int scale_rc = sws_scale(rgb_to_yuv_SwsContext, inData, inLinesize, 0, inframe->height, outframe->data,
			outframe->linesize);
		m1.unlock();
		stop = clock();
		nDiff = stop - start;
		//fprintf(file, "time for scale - %d\n", nDiff);
		time_for_scaling_rgb_to_yuv += nDiff;
		number_of_rgb_frames_scaled++;

		outframe->pts = i;
		
		pDataArray[0] = (PMYDATA)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
			sizeof(MYDATA));
		pDataArray[1] = (PMYDATA)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
			sizeof(MYDATA));
		pDataArray[2] = (PMYDATA)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
			sizeof(MYDATA));
		pDataArray[3] = (PMYDATA)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
			sizeof(MYDATA));
		pDataArray[4] = (PMYDATA)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
			sizeof(MYDATA));
		pDataArray[5] = (PMYDATA)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
			sizeof(MYDATA));
		/* encode the image */
		start = clock();

		pDataArray[0]->frame = outframe;
		pDataArray[0]-> p=pkt;
		pDataArray[0]->av= encodingCodecContext;
		pDataArray[0]->file= h264_file;
		pDataArray[1]->frame = outframe;
		pDataArray[1]->p = pkt1;
		pDataArray[1]->av = encodingCodecContext1;
		pDataArray[1]->file = h264_file1;
		pDataArray[2]->frame = outframe;
		pDataArray[2]->p = pkt2;
		pDataArray[2]->av = encodingCodecContext2;
		pDataArray[2]->file = h264_file2;
		pDataArray[3]->frame = outframe;
		pDataArray[3]->p = pkt3;
		pDataArray[3]->av = encodingCodecContext3;
		pDataArray[3]->file = h264_file3;
		pDataArray[4]->frame = outframe;
		pDataArray[4]->p = pkt4;
		pDataArray[4]->av = encodingCodecContext4;
		pDataArray[4]->file = h264_file4;
		pDataArray[5]->frame = outframe;
		pDataArray[5]->p = pkt5;
		pDataArray[5]->av = encodingCodecContext5;
		pDataArray[5]->file = h264_file5;
		for (int i = 0; i < 6; i++)
		{
			hThreadArray[i] = CreateThread(NULL, 0, encode, pDataArray[i], 0, &dwThreadIdArray[i]);
			if (hThreadArray[i] == NULL)
			{
				std::cout << "fsdz";

			}

			
		}
		/*std::thread th1(encode, outframe, pkt, encodingCodecContext, h264_file);
		std::thread th2(encode, outframe, pkt1, encodingCodecContext1, h264_file1);
		std::thread th3(encode, outframe, pkt2, encodingCodecContext2, h264_file2);
		std::thread th4(encode, outframe, pkt3, encodingCodecContext3, h264_file3);
		std::thread th5(encode, outframe, pkt4, encodingCodecContext4, h264_file4);
		std::thread th6(encode, outframe, pkt5, encodingCodecContext5, h264_file5);*/
		
		/*th1.join();
		th2.join();
		th3.join();
		th4.join();
		th5.join();
		th6.join();*/
		stop = clock();
		nDiff = stop - start;
		time_for_encode += nDiff;
		number_of_frames_encoded++;
		//fprintf(file, "time for encode - %d\n", nDiff);

	}
	
	WaitForMultipleObjects(6, hThreadArray, TRUE, INFINITE);
	for (int i = 0; i < 6; i++)
	{
		CloseHandle(hThreadArray[i]);

	}
	/* flush the encoder */
	start = clock();
	/*pDataArray->frame = NULL;
	pDataArray->p = pkt;
	pDataArray->av = encodingCodecContext;
	pDataArray->file = h264_file;
	HANDLE myHandle = CreateThread(NULL, 0, encode, pDataArray, 0, &myThreadID);
	CloseHandle(myHandle);*/
	//encode(NULL, pkt, encodingCodecContext, h264_file);
	/*encode(NULL, pkt1, encodingCodecContext1, h264_file1);
	encode(NULL, pkt2, encodingCodecContext2, h264_file2);
	encode(NULL, pkt3, encodingCodecContext3, h264_file3);
	encode(NULL, pkt4, encodingCodecContext4, h264_file4);
	encode(NULL, pkt5, encodingCodecContext5, h264_file5);*/
	stop = clock();
	nDiff = stop - start;
	time_for_encode += nDiff;
	number_of_frames_encoded++;

	/* add sequence end code to have a real MPEG file */
	fwrite(endcode, 1, sizeof(endcode), h264_file);
	fclose(h264_file);
	fwrite(endcode, 1, sizeof(endcode), h264_file1);
	fclose(h264_file1);
	fwrite(endcode, 1, sizeof(endcode), h264_file2);
	fclose(h264_file2);
	fwrite(endcode, 1, sizeof(endcode), h264_file3);
	fclose(h264_file3);
	fwrite(endcode, 1, sizeof(endcode), h264_file4);
	fclose(h264_file4);
	fwrite(endcode, 1, sizeof(endcode), h264_file5);
	fclose(h264_file5);


	/*fprintf(log_file, "average time for scale - %f\n", time_for_scaling_rgb_to_yuv / (float)number_of_rgb_frames_scaled);
	fprintf(log_file, "average time for encode - %f\n", time_for_encode / (float)number_of_frames_encoded);
	fprintf(log_file, "average time for decode - %f\n", time_for_decode / (float)number_of_packets_decoded);
	fprintf(log_file, "average time for capture - %f\n", time_for_capture / (float)number_of_frames_captured);
	fprintf(log_file, "average size after encoding - %f\n", total_size_of_packets / (float)number_of_packets);
	is_keyframe[decodingCodecContext->frame_number] = '\0';
	fwrite(is_keyframe, 1, decodingCodecContext->frame_number + 1, log_file);
	fprintf(log_file, "\n", NULL);
	fprintf(log_file, "Number of I - frames : %d\n", number_of_I_frames);
	fprintf(log_file, "Number of P - frames : %d\n", number_of_packets);*/

}
//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE:  Processes messages for the main window.
//
//  WM_COMMAND  - process the application menu
//  WM_PAINT    - Paint the main window
//  WM_DESTROY  - post a quit message and return
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{

	case WM_DESTROY:
		Cleanup();
		PostQuitMessage(0);
		return 0;
	case WM_SIZE:
		return 0;
	}
	return DefWindowProc(hWnd, message, wParam, lParam);
}

// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);
	switch (message)
	{
	case WM_INITDIALOG:
		return (INT_PTR)TRUE;

	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
		{
			EndDialog(hDlg, LOWORD(wParam));
			return (INT_PTR)TRUE;
		}
		break;
	}
	return (INT_PTR)FALSE;
}