// Downloaded from https://developer.x-plane.com/code-sample/commandsim/


/*
 * CommandSim.c
 * 
 * This function demonstrates how to send commands to the sim.  Commands allow you to simulate
 * any keystroke or joystick button press or release.
 * 
 */

#include <stdio.h>
#include <string.h>
#include "XPLMMenus.h"
#include "XPLMUtilities.h"
#include "json-c/json_tokener.h"
#include "json-c/json_object.h"

enum {
	request_dataitems=0,
	start_close_loop,
	stop_close_loop
};

static void	MyMenuHandlerCallback(
                                   void *               inMenuRef,    
                                   void *               inItemRef); 
static void sendJSONRequest(void);

PLUGIN_API int XPluginStart(
						char *		outName,
						char *		outSig,
						char *		outDesc)
{
	XPLMMenuID	myMenu;
	int			mySubMenuItem;

	strcpy(outName, "CommandSim");
	strcpy(outSig, "xplanesdk.examples.commandsim");
	strcpy(outDesc, "A plugin that makes the command do things.");
	
	/* Create a menu for ourselves.  */
	mySubMenuItem = XPLMAppendMenuItem(
						XPLMFindPluginsMenu(),	/* Put in plugins menu */
						"JSONConnector",		/* Item Title */
						0,						/* Item Ref */
						1);						/* Force English */
	
	myMenu = XPLMCreateMenu(
						"JSONConnector", 
						XPLMFindPluginsMenu(), 
						mySubMenuItem, 			/* Menu Item to attach to. */
						MyMenuHandlerCallback,	/* The handler */
						0);						/* Handler Ref */

	/* For each command, we set the item refcon to be the key command ID we wnat
	 * to run.   Our callback will use this item refcon to do the right command.
	 * This allows us to write only one callback for the menu. */	 
	XPLMAppendMenuItem(myMenu, "Start JSONControl", (void *) start_close_loop, 1);
	XPLMAppendMenuItem(myMenu, "Stop JSONControl", (void *) stop_close_loop, 1);
	XPLMAppendMenuItem(myMenu, "Request DataItems", (void *) request_dataitems, 1);
	return 1;
}

PLUGIN_API void	XPluginStop(void)
{
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

void	MyMenuHandlerCallback(
                                   void *               inMenuRef,    
                                   void *               command)
{
	
	if (*(int*)command == request_dataitems) {
		XPLMDebugString("RequestDataItems");
		sendJSONRequest();
	}
	else if (*(int*)command == start_close_loop) {
		XPLMDebugString("StartCloseLoop");
	}
	else if (*(int*)command == stop_close_loop) {
		XPLMDebugString("StopCloseLoop");
	}
	/*
	char buffer[256] = "";
	sprintf(buffer, "%i", (*(int*)command) );
	strcat(buffer, "Test");
	XPLMDebugString(buffer);*/
	
	//XPLMCommandKeyStroke((XPLMCommandKeyID) inItemRef);
}

void sendJSONRequest(void) {
	XPLMDebugString("Sending JSONRequest");
	json_object *jsonObject;
	jsonObject = json_tokener_parse("{\"JSONDebugData\":[[\"Periodic\",\"SYS_ControlHandler_Context\"]]}");
}

