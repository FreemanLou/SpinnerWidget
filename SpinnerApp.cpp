/*
 * Copyright 2013 Freeman Lou, <freemanlou2430@yahoo.com>
 * All rights reserved. Distributed under the terms of the MIT license.
 */
#include "SpinnerApp.h"

#include <Layout.h>
#include <LayoutBuilder.h>
#include <GroupView.h>
#include <GroupLayout.h>
#include <GroupLayoutBuilder.h>
#include <Window.h>

#include "Spinner.h"

SpinnerApp::SpinnerApp()
	:
	BApplication("application/x-vnd.SpinControl")
{
	BRect frame(50,50,200,100);
	BWindow* window = new BWindow(frame,"SpinControl", B_TITLED_WINDOW,
		B_QUIT_ON_WINDOW_CLOSE );//| B_NOT_RESIZABLE | B_NOT_ZOOMABLE);
	
	Spinner* spinner = new Spinner("Spinner",
		"Variable: ", NULL, B_WILL_DRAW);
	
	spinner->SetExplicitMinSize(BSize(60,60));
	spinner->SetRange(-500, 500);
	spinner->SetValue(0);
	spinner->SetSteps(5);

	BLayoutBuilder::Group<>(window, B_VERTICAL, 0)
		.Add(spinner)
	.End();
	window->Show();
}


int main()
{
	SpinnerApp* app = new SpinnerApp();
	app->Run();

	delete app;
	return 0;
}
