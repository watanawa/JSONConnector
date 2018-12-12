// JSONConnector.cpp: Definiert die exportierten Funktionen für die DLL-Anwendung.
//
#include <string.h>
#include <string>
#include <stdio.h>
#include <winsock2.h>
#include <Windows.h>
#include <process.h>
#include "XPLMUtilities.h"
#include "XPLMGraphics.h"
#include "XPLMDisplay.h"
#include "XPLMMenus.h"
#include "nlohmann/json.hpp"

#define PORT 4004
#define PROXYPORT 4002

#pragma comment (lib, "Ws2_32.lib")
#pragma comment (lib, "Mswsock.lib")
#pragma comment (lib, "AdvApi32.lib")

using json = nlohmann::json;

//FUNCTIONDECLARATIONS
void myMenuHandler(void * in_menu_ref, void * in_item_ref);
void sendJsonRequest(void);
void openSocket();
void initializeDestinationAddress();
void createMenu();
void startLoopControl();
void stopLoopControl();
void createThreads();
unsigned int processJSONMessage(void *);
unsigned int outputJSONMessage(void *);

//GLOBAL VARIABLES
int g_menu_container_idx; // The index of our menu item in the Plugins menu
XPLMMenuID g_menu_id; // The menu container we'll append all our menu items to
SOCKET receivingSocket; //UDP SOCKET
SOCKADDR_IN  destinationAddr;
WSADATA wsaData;
int bindingSuccesful = 0;
HANDLE processJSONThread;
HANDLE outputJSONThread;
int endThreads = 0;
static char datarefControls[][256] = {  "sim/flightmodel/controls/vectrqst",        //COMMANDED THRUST
										"sim/flightmodel/controls/wing1l_ail1def",	//WING 1 LEFT AILERON
										"sim/flightmodel/controls/wing1r_ail1def",  //WING 1 RIGHT AILERON
										"sim/flightmodel/controls/wing1l_fla1def",  //WING 1 LEFT FLAP
										"sim/flightmodel/controls/wing1r_fla1def",	//WING 1 RIGHT FLAP
										"sim/flightmodel/controls/hstab1_elv1def",	//Wing 1 ELEVATOR 1
										"sim/flightmodel/controls/hstab2_elv1def",  //Wing 2 ELEVATOR 1 - Same value as the above
										"sim/flightmodel/controls/vstab1_rud1def"   //Rudder theres just 1
};
static char datarefOutputs[][256] = {	"sim/flightmodel/position/local_ax",		//X ACCELARATION		m/s^2	double
										"sim/flightmodel/position/local_ay",		//Y ACCELARATION		m/s^2	double
										"sim/flightmodel/position/local_az",		//Z ACCELARATION		m/s^2	double
										"sim/flightmodel/position/Prad",			//ROLLRATE				rad/s	float
										"sim/flightmodel/position/Qrad",			//PITCHRATE				rad/s	float
										"sim/flightmodel/position/Rrad",			//YAWRATE				rad/s	float
										"sim/flightmodel/position/phi",				//ROLL					DEGREES	float
										"sim/flightmodel/position/theta",			//PITCH					DEGRESS float
										"sim/flightmodel/position/psi",				//YAW					DEGRESS float
										//SPEEDNORTH
										//SPEEDEAST
										//SPEEDDOWN
										"sim/flightmodel/position/latitude",		//LATITUDE				DEGREES	double
										"sim/flightmodel/position/longitude",		//LONGITUDE				DEGREES	double
										"sim/flightmodel/position/elevation",		//ELEVATION ABOVE MSL	m		double
										//ALTITUDEAGL
										//COURSE OVER GROUND
										"sim/flightmodel/position/groundspeed",		//GROUNDSPEED			m/s		float
										"sim/flightmodel/failures/onground_any"		//AIRCRAFT IN GROUND			int 1=groundcontact
										

};				


//Windows necessary
BOOL APIENTRY DllMain(HANDLE hModule,
	DWORD ul_reason_for_call,
	LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
	case DLL_PROCESS_DETACH:
		break;
	}
	return TRUE;
}

//XPLANE FUNCTIONS
PLUGIN_API int XPluginStart(char * outName, char * outSignature, char * outDescription){
	strcpy(outName,"JsonConnector");
	strcpy(outSignature,"AIRBUS.SCHWIPPS.JsonConnector");
	strcpy(outDescription,"Closed loop to DSFProxy");
	
	createMenu();
	openSocket();
	initializeDestinationAddress();
	createThreads();

	return 1;
}

PLUGIN_API void	XPluginStop(void)
{
	closesocket(receivingSocket);
	WSACleanup();
	XPLMDestroyMenu(g_menu_id);
	endThreads = 1;
	WaitForSingleObject(processJSONThread, INFINITE);
	WaitForSingleObject(outputJSONThread, INFINITE);
}

PLUGIN_API void XPluginDisable(void)
{
}

PLUGIN_API int XPluginEnable(void)
{
	return 1;
}

PLUGIN_API void XPluginReceiveMessage(
	XPLMPluginID	inFromWho,
	int				inMessage,
	void *			inParam)
{
}

//INTERNAL FUNCTIONS
void createMenu() {
	g_menu_container_idx = XPLMAppendMenuItem(XPLMFindPluginsMenu(), "JSONConnector", 0, 0);
	g_menu_id = XPLMCreateMenu("Sample Menu", XPLMFindPluginsMenu(), g_menu_container_idx, myMenuHandler, NULL);
	XPLMAppendMenuItem(g_menu_id, "Send JSON Request", (void *)"Send JSON Request", 1);
	XPLMAppendMenuItem(g_menu_id, "Start loop control", (void *)"Start loop", 1);
	XPLMAppendMenuItem(g_menu_id, "Stop loop control" , (void *)"Stop loop", 1);
}


void myMenuHandler(void * in_menu_ref, void * in_item_ref) {
	if (!strcmp((const char *)in_item_ref, "Send JSON Request"))
	{
		XPLMDebugString("SENDING JSON REQUEST\n");
		sendJsonRequest();
	}
	else if (!strcmp((const char *)in_item_ref, "Start loop"))
	{
		XPLMDebugString("STARTING LOOP CONTROL\n");
		startLoopControl();
	}
	else if (!strcmp((const char *)in_item_ref, "Stop loop")) {
		XPLMDebugString("STOPPING LOOP CONTROL\n");
		stopLoopControl();
	}
	else {
		XPLMDebugString("Unidentified menuitem");
	}
}

void sendJsonRequest(void) {
	json jsonObject;
	json jsonArray = json::array({ { "Periodic" } ,{ "SYS_ControlHandler_Context" } });
	jsonObject["JSONDebugDataReadRequest"] = jsonArray;
	std::string jsonString = jsonObject.dump();
	int msgSent = sendto(receivingSocket, jsonString.c_str(),(int) jsonString.length(),0, (SOCKADDR *)&destinationAddr,sizeof(destinationAddr));
}
void openSocket() {
wsaData;
SOCKADDR_IN  receivingAddr;

int iResult;

// Initialize Winsock
iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
char temp[100];

if (iResult != 0) {
	sprintf(temp,"WSAStartup failed: %d\n", iResult);
	XPLMDebugString(temp);
	return;
}

receivingSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
if (receivingSocket == INVALID_SOCKET)
	{
		char temp[100];
		sprintf(temp,"Server: Error at socket(): %ld\n", WSAGetLastError());
		XPLMDebugString(temp);
		WSACleanup();
		// Exit with error
		return;
	}

hostent* localHost = gethostbyname("");
char* localIP = inet_ntoa(*(struct in_addr *)*localHost->h_addr_list);

receivingAddr.sin_family = AF_INET;
receivingAddr.sin_port = htons(PORT);
receivingAddr.sin_addr.s_addr = inet_addr(localIP);

// At this point you can receive datagrams on your bound socket.
if (bind(receivingSocket, (SOCKADDR *)&receivingAddr, sizeof(receivingAddr)) == SOCKET_ERROR)
{
	sprintf(temp, "Binding error %i", WSAGetLastError());
	XPLMDebugString(temp);
	closesocket(receivingSocket);
	WSACleanup();
	return ;
}
sprintf(temp, "BINDING SOCKET TO %i PORT:%i WAS SUCCESSFUL\n", inet_addr(localIP), PORT);
XPLMDebugString(temp);
bindingSuccesful = 1;
return;
}
void initializeDestinationAddress() {
	hostent* localHost = gethostbyname("");
	char* localIP = inet_ntoa(*(struct in_addr *)*localHost->h_addr_list);

	destinationAddr.sin_family = AF_INET;
	destinationAddr.sin_port = htons(PROXYPORT);
	destinationAddr.sin_addr.s_addr = inet_addr(localIP);

}
void createThreads() {
	processJSONThread = (HANDLE) _beginthreadex(NULL, 0, &processJSONMessage, NULL, CREATE_SUSPENDED, 0);
	outputJSONThread = (HANDLE)_beginthreadex(NULL, 0, &outputJSONMessage, NULL, CREATE_SUSPENDED, 0);
}
void startLoopControl() {
	ResumeThread(processJSONThread);
	ResumeThread(outputJSONThread);
}
void stopLoopControl() {
	SuspendThread(processJSONThread);
	SuspendThread(outputJSONThread);
}

unsigned int processJSONMessage(void * dummy) {
	while (endThreads != 1) {
		
		Sleep(200L);
	}
	return 0;
}
unsigned int outputJSONMessage(void * dummy) {
	long i = 0;
	while (endThreads != 1) {
		char temp[100];
		sprintf(temp, "Threadtest %i", i);
		XPLMDebugString(temp);
		i++;
		Sleep(200L);
	}
	return 0;
}
