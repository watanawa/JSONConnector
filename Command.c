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
#include "XPLMDataAccess.h"
#include "nlohmann/json.hpp"

#define PORT 4004
#define PROXYPORT 4002
#define Pi 3.14159265358979323846

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
void createThread();
unsigned int jsonLoopControl(void * dummy);
void processJSONMessage(json jsonObject);
void outputJSONMessage();
void initializeDatarefs();

//GLOBAL VARIABLES
int g_menu_container_idx; // The index of our menu item in the Plugins menu
XPLMMenuID g_menu_id; // The menu container we'll append all our menu items to
SOCKET receivingSocket; //UDP SOCKET
SOCKADDR_IN  destinationAddr;
WSADATA wsaData;
int bindingSuccesful = 0;
HANDLE jsonCloseLoopThread;
int endThreads = 0;

static char datarefNameOverride[][100] = {	"sim/operation/override/override_throttles",			//OVERRDIDE THROTLE FLAG						int	0/1
											"sim/operation/override/override_control_surfaces"		//OVERRIDE CONTROL SURFACES						int 0/1
};
static XPLMDataRef datarefOverride[2] = {NULL};

//Control arrays
static char datarefNameControls[][256] = {  "sim/flightmodel/controls/vectrqst",					//COMMANDED THRUST								float 0..1
											"sim/flightmodel/controls/wing1l_ail1def",				//WING 1 LEFT AILERON							float DEGREES
											"sim/flightmodel/controls/wing1r_ail1def",				//WING 1 RIGHT AILERON							float DEGREES
											"sim/flightmodel/controls/wing1l_fla1def",				//WING 1 LEFT FLAP								float DEGREES
											"sim/flightmodel/controls/wing1r_fla1def",				//WING 1 RIGHT									float DEGREES
											"sim/flightmodel/controls/hstab1_elv1def",				//Wing 1 ELEVATOR 1								float DEGREES
											"sim/flightmodel/controls/hstab2_elv1def",				//Wing 2 ELEVATOR 1 - Same value as the above	float DEGREES
											"sim/flightmodel/controls/vstab1_rud1def",				//Rudder theres just + means right				float DEGREES
											"sim/flightmodel/controls/vstab2_rud1def"				//Rudder 2 for completion
};
static XPLMDataRef datarefControls[10] = { NULL };

static char jsonControlVariableName[] = "SYS_ControlHandler_Context";
static char jsonControlName[][100] = {	"PusherMotor",
										"AileronLeft", //POSITIVE MEANS UP
										"AileronRight",//POISTIVE MEANS DOWN
										"FlapLeft",		
										"FlapRight",	
										"ElevatorLeft", //POSITIVE DOWN
										"ElevatorRight",//POSITIVE DOWN
										"RudderLeft",	//POSITIVE LEFT
										"RudderRight", //POISTIVE LEFT
										};
static double jsonControlConversion[] = {	1,
											-180/Pi*(1/20),
											-180/Pi*(1/20),
											180/Pi,
											180/Pi,
											-180/Pi*(1/20),
											-180/Pi*(1/20),
											-180/Pi*(1/20),
											-180/Pi*(1/20),
};

//Output arrays
static char datarefNameOutputs[][100] = {	"sim/flightmodel/forces/g_axil",			//ACCELERATION FRONT g	m/s^2	float
											"sim/flightmodel/forces/g_side",			//ACCELERATION RIGHT g	m/s^2	float
											"sim/flightmodel/forces/g_nrml",			//ACCELERATION DOWN	 g	m/s^2	float
											"sim/flightmodel/position/Prad",			//ROLLRATE				rad/s	float
											"sim/flightmodel/position/Qrad",			//PITCHRATE				rad/s	float
											"sim/flightmodel/position/Rrad",			//YAWRATE				rad/s	float
											"sim/flightmodel/position/true_phi",		//ROLL					DEGREES	float
											"sim/flightmodel/position/true_theta",		//PITCH					DEGRESS float
											"sim/flightmodel/position/true_psi",		//YAW					DEGRESS float
											"sim/flightmodel/position/local_vz",		//SPEED SOUTH			m/s		float
											"sim/flightmodel/position/local_vx",		//SPEED EAST			m/s		float
											"sim/flightmodel/position/local_vy",		//SPEED UP				m/s		float
											"sim/flightmodel/position/latitude",		//LATITUDE				DEGREES	double
											"sim/flightmodel/position/longitude",		//LONGITUDE				DEGREES	double
											"sim/flightmodel/position/elevation",		//ELEVATION ABOVE MSL	m		double
											"sim/flightmodel/position/y_agl",			//ALTITUDE ABOVE GROUND	m		float
											"sim/flightmodel/position/true_psi",		//Heading				DEGREES	float
											"sim/flightmodel/position/groundspeed",		//GROUNDSPEED			m/s		float
											"sim/flightmodel/failures/onground_any"		//AIRCRAFT IN GROUND			int 1=groundcontact


};
static XPLMDataRef datarefOutputs[19] = { NULL };
static char jsonOutputVariableName[] = "SIM_AhrsGpsModel_Context";
static char jsonOutputName[][100] = {	"AccelerationX",
										"AccelerationY",
										"AccelerationZ",
										"RollRate",
										"PitchRate",
										"YawRate",
										"Roll",
										"Pitch",
										"Yaw",
										"SpeedNorth",
										"SpeedEast",
										"SpeedDown",
										"LatitudeWgs84",
										"LongitudeWgs84",
										"AltitudeWgs84Geoid",
										"AltitudeAgl",
										"CourseOverGround",
										"SpeedOverGround",
										"TouchdownFlag"
};
static double jsonOutputConversion[] = {-9.81,		// g->m/s^2		
										-9.81,		// g->m/s^2
										-9.81,		// g->m/s^2
										1,			
										1,
										1,
										Pi / 180,	//Degrees->Rad
										Pi / 180,	//Degrees->Rad
										Pi / 180,	//Degrees->Rad
										-1,			//Vsouth -> Vnorth
										1,			
										-1,			//Vup	 -> Vdown
										Pi/180,		//Degrees->Rad
										Pi/180,		//Degrees->Rad
										1,
										1,
										Pi / 180,	//DEGRESS->Rad
										1,
										1,
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
	createThread();

	initializeDatarefs();
	return 1;
}

PLUGIN_API void	XPluginStop(void)
{
	closesocket(receivingSocket);
	WSACleanup();
	XPLMDestroyMenu(g_menu_id);
	endThreads = 1;
	startLoopControl();
	WaitForSingleObject(jsonCloseLoopThread, 200L);
	CloseHandle(jsonCloseLoopThread);
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
	json jsonArray = json::array({{"Periodic","SYS_ControlHandler_Context"}});
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
void createThread() {
	jsonCloseLoopThread = (HANDLE)_beginthreadex(NULL, 0, &jsonLoopControl, NULL, CREATE_SUSPENDED, 0);
}
void startLoopControl() {
	//Throttle
	XPLMSetDatai(datarefOverride[0],1);
	//ControlSurface
	XPLMSetDatai(datarefOverride[1],1);
	ResumeThread(jsonCloseLoopThread);
}
void stopLoopControl() {
	//Throttle
	XPLMSetDatai(datarefOverride[0], 0);
	//ControlSurface
	XPLMSetDatai(datarefOverride[1], 0);
	SuspendThread(jsonCloseLoopThread);
}


unsigned int jsonLoopControl(void * dummy) {
	char recvBuffer[6144];
	while (endThreads == 0) { 
		
		//Wait for receive
		int result = recv(receivingSocket,recvBuffer,4048, 0 );
		//Error case
		if (result < 0) {
			char temp[256] = "";
			int errorCode = WSAGetLastError();
			sprintf(temp, "Receiving error: %i\n", errorCode);
			XPLMDebugString(temp);
		}
		else {
			char* jsonString = (char*)calloc(result, 1);
			memcpy(jsonString,recvBuffer,result);
			json jsonObject;
			try {
				jsonObject = json::parse(jsonString);
			}
			catch (json::parse_error& e)
			{
				XPLMDebugString(e.what());
			}
			processJSONMessage(jsonObject);
			free(jsonString);
		}
		outputJSONMessage();
		Sleep(10L);
	}
	return 0;
}
void processJSONMessage(json jsonObject) {
	float controls[9] = { NULL };

	//SET THESE VALUES FROM JSON MESSAGE
	try {
		for (int i = 0; i < 9; i++) {
			controls[i] = jsonObject["JSONDebugDataMessage"][jsonControlVariableName][jsonControlName[i]].get<float>();
		}
	}
	//ERROR
	catch(json::type_error& e){
		XPLMDebugString(e.what());
		return;
	}
	for (int i = 0; i < 9; i++) {
		XPLMSetDataf(datarefControls[i], ((float)controls[i]*jsonControlConversion[i]));
	}
}
void outputJSONMessage() {
	json jsonOutput;
	std::vector<json> jsonSubArray(19);
	try {
		for (int i = 0; i < 19; i++) {
			if (i <= 14 && i >= 12) {
				jsonSubArray[i] = json::array({jsonOutputVariableName,jsonOutputName[i],XPLMGetDatad(datarefOutputs[i])*jsonOutputConversion[i]});
			}
			else if (i == 18) {
				jsonSubArray[i] = json::array({jsonOutputVariableName,jsonOutputName[i],XPLMGetDatai(datarefOutputs[i])*jsonOutputConversion[i]});
			}
			else {
				jsonSubArray[i] = json::array({jsonOutputVariableName,jsonOutputName[i],((double)XPLMGetDataf(datarefOutputs[i])*jsonOutputConversion[i])});
			}
		}
		jsonOutput["JSONDebugDataWriteRequest"] = jsonSubArray;
	}
	catch (json::type_error& e) {
		XPLMDebugString(e.what());
		return;
	}
	//XPLMDebugString(jsonOutput.dump().c_str());
	//XPLMDebugString("\n");
	int msgSent = sendto(receivingSocket, jsonOutput.dump().c_str(), (int)jsonOutput.dump().length(), 0, (SOCKADDR *)&destinationAddr, sizeof(destinationAddr));
}

void initializeDatarefs(){
	for (int i = 0; i < 9; i++) {
		datarefControls[i] = XPLMFindDataRef(datarefNameControls[i]);
	}
	for (int i = 0; i < 19; i++ ) {
		datarefOutputs[i] = XPLMFindDataRef(datarefNameOutputs[i]);
	}
	for (int i = 0; i < 2; i++) {
		datarefOverride[i] = XPLMFindDataRef(datarefNameOverride[i]);
	}
}
