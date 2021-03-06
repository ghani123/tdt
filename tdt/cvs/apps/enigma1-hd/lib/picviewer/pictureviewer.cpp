/*
 * $Id: pictureviewer.cpp,v 1.47 2009/02/03 18:53:43 dbluelle Exp $
 *
 * (C) 2005 by digi_casi <digi_casi@tuxbox.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */
 
#include <config.h>
#include <lib/driver/streamwd.h>
#include <lib/system/info.h>
#include <lib/system/init.h>
#include <lib/system/init_num.h>
#include <lib/system/econfig.h>
#include <lib/gdi/fb.h>
#include <lib/base/estring.h>
#include <lib/gui/actions.h>
#include <lib/gui/guiactions.h>
#include <lib/dvb/servicedvb.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <dirent.h>
#include <src/enigma_main.h>
#include <lib/gui/eskin.h>
#include <lib/gui/elabel.h>
#include <lib/gdi/font.h>
#include <lib/picviewer/pictureviewer.h>
#include "fb_display.h"
#include <lib/picviewer/format_config.h>
#include <lib/driver/eavswitch.h>

/* resize.cpp */
extern unsigned char *simple_resize(unsigned char *orgin, int ox, int oy, int dx, int dy);
extern unsigned char *color_average_resize(unsigned char *orgin, int ox, int oy, int dx, int dy);

#ifdef FBV_SUPPORT_GIF
extern int fh_gif_getsize(const char *, int *, int *, int, int);
extern int fh_gif_load(const char *, unsigned char *, int, int);
extern int fh_gif_id(const char *);
#endif
#ifdef FBV_SUPPORT_JPEG
extern int fh_jpeg_getsize(const char *, int *, int *, int, int);
extern int fh_jpeg_load(const char *, unsigned char *, int, int);
extern int fh_jpeg_id(const char *);
#endif
#ifdef FBV_SUPPORT_PNG
extern int fh_png_getsize(const char *, int *, int *, int, int);
extern int fh_png_load(const char *, unsigned char *, int, int);
extern int fh_png_id(const char *);
#endif
#ifdef FBV_SUPPORT_BMP
extern int fh_bmp_getsize(const char *, int *, int *, int, int);
extern int fh_bmp_load(const char *, unsigned char *, int, int);
extern int fh_bmp_id(const char *);
#endif
#ifdef FBV_SUPPORT_CRW
extern int fh_crw_getsize(const char *, int *, int *, int, int);
extern int fh_crw_load(const char *, unsigned char *, int, int);
extern int fh_crw_id(const char *);
#endif

void ePictureViewer::nextPicture()
{
	if (++myIt == slideshowList.end())
		myIt = slideshowList.begin();
}

void ePictureViewer::previousPicture()
{
	if (myIt == slideshowList.begin())
	{
		myIt = slideshowList.end();
		myIt--;
	}
	else
		myIt--;
}

void ePictureViewer::showNameOnLCD(const eString& filename)
{
	unsigned int pos = filename.find_last_of("/");
	pos = (pos == eString::npos) ? 0 : pos + 1;
	eString name = filename.substr(pos, filename.length() - 1);
#ifndef DISABLE_LCD
	pLCD->lcdMain->ServiceName->setText(name);
	pLCD->lcdMain->show();
#endif
}

ePictureViewer::ePictureViewer(const eString &filename)
	:eWidget(0, 1), slideshowTimer(eApp), filename(filename)
{
	init_ePictureViewer();
}
void ePictureViewer::init_ePictureViewer()
{
	eDebug("[PICTUREVIEWER] Constructor...");

	Decoder::clearScreen();
	addActionMap(&i_cursorActions->map);
	addActionMap(&i_shortcutActions->map);
	addActionToHelpList(i_shortcutActions->blue.setDescription(_("display next picture")));
	addActionToHelpList(i_shortcutActions->red.setDescription(_("display previous picture")));
	addActionToHelpList(i_shortcutActions->green.setDescription(_("start slideshow")));
	addActionToHelpList(i_shortcutActions->yellow.setDescription(_("stop slideshow")));
	addActionToHelpList(i_shortcutActions->number0.setDescription(_("toggle fit to screen width/fit to screen")));
	addActionToHelpList(i_shortcutActions->number1.setDescription(_("zoom out")));
	addActionToHelpList(i_shortcutActions->number3.setDescription(_("zoom in")));
	addActionToHelpList(i_shortcutActions->number5.setDescription(_("toggle 16/9 mode")));
	addActionToHelpList(i_shortcutActions->number2.setDescription(_("move picture up")));
	addActionToHelpList(i_shortcutActions->number4.setDescription(_("move picture left")));
	addActionToHelpList(i_shortcutActions->number6.setDescription(_("move picture right")));
	addActionToHelpList(i_shortcutActions->number8.setDescription(_("move picture down")));

	move(ePoint(70, 50));
	resize(eSize(590, 470));
	eLabel *l = new eLabel(this);
	l->move(ePoint(150, clientrect.height() / 2));
	l->setFont(eSkin::getActive()->queryFont("epg.title"));
	l->resize(eSize(clientrect.width() - 100, 30));
	l->setText(_("Loading slide... please wait."));

#ifndef DISABLE_LCD
	pLCD = eZapLCD::getInstance();
#endif

	fh_root = NULL;
	m_scaling = COLOR;

	m_NextPic_Name = "";
	m_NextPic_Buffer = NULL;
	m_NextPic_X = 0;
	m_NextPic_Y = 0;
	m_NextPic_XPos = 0;
	m_NextPic_YPos = 0;
	m_NextPic_XPan = 0;
	m_NextPic_YPan = 0;
	m_bFitScreen = false;

	m_startx = 20, m_starty = 20, m_endx = 699, m_endy = 555;
	eConfig::getInstance()->getKey("/enigma/plugins/needoffsets/left", m_startx); // left
	eConfig::getInstance()->getKey("/enigma/plugins/needoffsets/top", m_starty); // top
	eConfig::getInstance()->getKey("/enigma/plugins/needoffsets/right", m_endx); // right
	eConfig::getInstance()->getKey("/enigma/plugins/needoffsets/bottom", m_endy); // bottom

	int showbusysign = 1;
	eConfig::getInstance()->getKey("/picviewer/showbusysign", showbusysign);
	showBusySign = (showbusysign == 1);
	unsigned int v_pin8 = 0;
	eConfig::getInstance()->getKey("/elitedvb/video/pin8", v_pin8);
	switchto43 = false;
	if ((v_pin8 == 3) || // always 16:9
		((v_pin8 > 1) && (eServiceInterface::getInstance()->getService()->getAspectRatio() == 1)))
	{
		m_aspect = 16.0 / 9;
	}
	else
	{
		m_aspect = 4.0 / 3;
		switchto43 = true;
	}

	format169 = 0;
	eConfig::getInstance()->getKey("/picviewer/format169", format169);
	if (format169)
	{
		eAVSwitch::getInstance()->setAspectRatio(r169);
		m_aspect = 16.0 / 9;
	}
	m_busy_buffer = NULL;

	init_handlers();

	CONNECT(slideshowTimer.timeout, ePictureViewer::slideshowTimeout);
	eDebug("[PICTUREVIEWER] Constructor done.");
}

ePictureViewer::~ePictureViewer()
{
	if (m_busy_buffer != NULL)
	{
		delete [] m_busy_buffer;
		m_busy_buffer = NULL;
	}
	if (m_NextPic_Buffer != NULL)
	{
		delete [] m_NextPic_Buffer;
		m_NextPic_Buffer = NULL;
	}

	CFormathandler *tmp = NULL;
	while(fh_root)
	{
		tmp = fh_root;
		fh_root = fh_root->next;
		delete tmp;
	}
// restore original screen size
	eStreamWatchdog::getInstance()->reloadSettings(); 
}

void ePictureViewer::add_format(int (*picsize)(const char *, int *, int *, int, int ), int (*picread)(const char *, unsigned char *, int, int), int (*id)(const char*))
{
	CFormathandler *fhn;
	fhn = new CFormathandler;
	fhn->get_size = picsize;
	fhn->get_pic = picread;
	fhn->id_pic = id;
	fhn->next = fh_root;
	fh_root = fhn;
}

void ePictureViewer::init_handlers(void)
{
#ifdef FBV_SUPPORT_GIF
	add_format(fh_gif_getsize, fh_gif_load, fh_gif_id);
#endif
#ifdef FBV_SUPPORT_JPEG
	add_format(fh_jpeg_getsize, fh_jpeg_load, fh_jpeg_id);
#endif
#ifdef FBV_SUPPORT_PNG
	add_format(fh_png_getsize, fh_png_load, fh_png_id);
#endif
#ifdef FBV_SUPPORT_BMP
	add_format(fh_bmp_getsize, fh_bmp_load, fh_bmp_id);
#endif
#ifdef FBV_SUPPORT_CRW
	add_format(fh_crw_getsize, fh_crw_load, fh_crw_id);
#endif
}

ePictureViewer::CFormathandler *ePictureViewer::fh_getsize(const char *name, int *x, int *y, int width_wanted, int height_wanted)
{
	CFormathandler *fh;
	for (fh = fh_root; fh != NULL; fh = fh->next)
	{
		if (fh->id_pic(name))
			if (fh->get_size(name, x, y, width_wanted, height_wanted) == FH_ERROR_OK)
				return(fh);
	}
	return(NULL);
}

bool ePictureViewer::DecodeImage(const std::string& name, bool unscaled)
{
	eDebug("DecodeImage {");

	int x, y, xs, ys, imx, imy;
	getCurrentRes(&xs, &ys);

	// Show red block for "next ready" in view state
	if (showBusySign)
		showBusy(m_startx + 3, m_starty + 3, 10, 0xff, 0, 0);

	CFormathandler *fh;
	if (unscaled || m_bFitScreen)
		fh = fh_getsize(name.c_str(), &x, &y, m_endx - m_startx, INT_MAX);
	else
		fh = fh_getsize(name.c_str(), &x, &y, m_endx - m_startx, m_endy - m_starty);
	if (fh)
	{
		if (m_NextPic_Buffer != NULL)
		{
			delete [] m_NextPic_Buffer;
		}
		m_NextPic_Buffer = new unsigned char[x * y * 3];
		if (m_NextPic_Buffer == NULL)
		{
			eDebug("Error: malloc");
			return false;
		}

		eDebug("---Decoding start(%d/%d)", x, y);
		if (fh->get_pic(name.c_str(), m_NextPic_Buffer, x, y) == FH_ERROR_OK)
		{
			eDebug("---Decoding done");
			if (m_bFitScreen || (((x > (m_endx - m_startx)) || y > (m_endy - m_starty)) && m_scaling != NONE && !unscaled))
			{
				double aspect_ratio_correction = m_aspect / ((double)xs / ys);
				if (m_bFitScreen || (aspect_ratio_correction * y * (m_endx - m_startx) / x) <= (m_endy - m_starty))
				{
					imx = (m_endx - m_startx);
					imy = (int)(aspect_ratio_correction * y * (m_endx - m_startx) / x);
				}
				else
				{
					imx = (int)((1.0 / aspect_ratio_correction) * x * (m_endy - m_starty) / y);
					imy = m_endy - m_starty;
				}
				if (m_scaling == SIMPLE)
					m_NextPic_Buffer = simple_resize(m_NextPic_Buffer, x, y, imx, imy);
				else
					m_NextPic_Buffer = color_average_resize(m_NextPic_Buffer, x, y, imx, imy);
				x = imx; y = imy;
			}
			m_NextPic_X = x;
			m_NextPic_Y = y;
			if (m_bFitScreen)
			{
				m_NextPic_XPos = m_startx;
				m_NextPic_YPos = m_starty;
				m_NextPic_XPan = 0;
				m_NextPic_YPan = 0;
			}
			else
			{
				if (x < (m_endx - m_startx))
					m_NextPic_XPos = (m_endx - m_startx - x) / 2 + m_startx;
				else
					m_NextPic_XPos = m_startx;
				if (y < (m_endy - m_starty))
					m_NextPic_YPos = (m_endy - m_starty-y) / 2 + m_starty;
				else
					m_NextPic_YPos = m_starty;
				if (x > (m_endx - m_startx))
					m_NextPic_XPan = (x - (m_endx - m_startx)) / 2;
				else
					m_NextPic_XPan = 0;
				if (y > (m_endy - m_starty))
					m_NextPic_YPan = (y - (m_endy - m_starty)) / 2;
				else
					m_NextPic_YPan = 0;
			}
		}
		else
		{
			eDebug("Unable to read file !");
			delete [] m_NextPic_Buffer;
			m_NextPic_Buffer = new unsigned char[3];
			if (m_NextPic_Buffer == NULL)
			{
				eDebug("Error: malloc");
				return false;
			}
			memset(m_NextPic_Buffer, 0 , 3);
			m_NextPic_X = 1;
			m_NextPic_Y = 1;
			m_NextPic_XPos = 0;
			m_NextPic_YPos = 0;
			m_NextPic_XPan = 0;
			m_NextPic_YPan = 0;
		}
	}
	else
	{
		eDebug("Unable to read file or format not recognized!");
		if (m_NextPic_Buffer != NULL)
		{
			delete [] m_NextPic_Buffer;
		}
		m_NextPic_Buffer = new unsigned char[3];
		if (m_NextPic_Buffer == NULL)
		{
			eDebug("Error: malloc");
			return false;
		}
		memset(m_NextPic_Buffer, 0 , 3);
		m_NextPic_X = 1;
		m_NextPic_Y = 1;
		m_NextPic_XPos = 0;
		m_NextPic_YPos = 0;
		m_NextPic_XPan = 0;
		m_NextPic_YPan = 0;
	}
	m_NextPic_Name = name;
	hideBusy();
	eDebug("DecodeImage }");
	return(m_NextPic_Buffer != NULL);
}

bool ePictureViewer::ShowImage(const std::string& filename, bool unscaled)
{
	eDebug("Show Image {");
	unsigned int pos = filename.find_last_of("/");
	if (pos == eString::npos)
		pos = filename.length() - 1;
	eString directory = pos ? filename.substr(0, pos) : "";
	eDebug("---directory: %s", directory.c_str());
	slideshowList.clear();
	int includesubdirs = 0;
	eConfig::getInstance()->getKey("/picviewer/includesubdirs", includesubdirs);
	listDirectory(directory, includesubdirs);
	for (myIt = slideshowList.begin(); myIt != slideshowList.end(); myIt++)
	{
		eString tmp = *myIt;
		eDebug("[PICTUREVIEWER] comparing: %s:%s", tmp.c_str(), filename.c_str());
		if (tmp == filename)
			break;
	}
	DecodeImage(filename, unscaled);
	struct fb_var_screeninfo *screenInfo = fbClass::getInstance()->getScreenInfo();
	if (screenInfo->bits_per_pixel != 16)
	{
		fbClass::getInstance()->lock();
		fbClass::getInstance()->SetMode(screenInfo->xres, screenInfo->yres, 16);
#if HAVE_DVB_API_VERSION >= 3
		fbClass::getInstance()->setTransparency(0);
#endif
	}
#ifndef DISABLE_LCD
	pLCD->lcdMenu->hide();
#endif
	showNameOnLCD(filename);
	DisplayNextImage();
	eDebug("Show Image }");
	return true;
}

void ePictureViewer::slideshowTimeout()
{
	eString tmp = *myIt;
	eDebug("[PICTUREVIEWER] slideshowTimeout: show %s", tmp.c_str());
	DecodeImage(*myIt, false);
	showNameOnLCD(tmp);
	DisplayNextImage();
	nextPicture();
	int timeout = 5;
	eConfig::getInstance()->getKey("/picviewer/slideshowtimeout", timeout);
	slideshowTimer.start(timeout * 1000, true);
}

int ePictureViewer::eventHandler(const eWidgetEvent &evt)
{

	static eString serviceName;
	fflush(stdout);
	struct fb_var_screeninfo *screenInfo = fbClass::getInstance()->getScreenInfo();	
        eDebug("[%s] evt.type = %d",__func__, evt.type);   
	switch(evt.type)
	{
		case eWidgetEvent::evtAction:
		{
			if (evt.action == &i_cursorActions->cancel)
				close(0);
			else
			if (evt.action == &i_cursorActions->help)
			{
				fbClass::getInstance()->SetMode(screenInfo->xres, screenInfo->yres, 32);
				fbClass::getInstance()->PutCMAP();
				fbClass::getInstance()->unlock();
				eWidget::eventHandler(evt);
				struct fb_var_screeninfo *screenInfo = fbClass::getInstance()->getScreenInfo();
				if (screenInfo->bits_per_pixel != 16)
				{

                         		fbClass::getInstance()->lock();
					fbClass::getInstance()->SetMode(screenInfo->xres, screenInfo->yres, 16);
#if HAVE_DVB_API_VERSION == 3
					fbClass::getInstance()->setTransparency(0);
#endif
				}
				DecodeImage(*myIt, false);
				showNameOnLCD(*myIt);
				DisplayNextImage();
				return 1;
			}
			else
			if (evt.action == &i_shortcutActions->yellow)
			{
				slideshowTimer.stop();
				previousPicture();
			}
			if (evt.action == &i_shortcutActions->green)
			{
				nextPicture();
				slideshowTimeout();
			}
			else
			if (evt.action == &i_shortcutActions->number0)
			{
				// Toggle FitToScreenWidth mode
				m_bFitScreen = !m_bFitScreen;
				DecodeImage(*myIt, false);
				DisplayNextImage();
			}
			else
			if (evt.action == &i_shortcutActions->number1)
			{
				m_bFitScreen=false;
				Zoom(2.0/3);
			}
			else
			if (evt.action == &i_shortcutActions->number2)
			{
				Move(0,-50);
			}
			else
			if (evt.action == &i_shortcutActions->number3)
			{
				Zoom(1.5);
			}
			else
			if (evt.action == &i_shortcutActions->number4)
			{
				Move(-50,0);
			}
			else
			if (evt.action == &i_shortcutActions->number5)
			{
				format169 = 1-format169;
				eConfig::getInstance()->setKey("/picviewer/format169", format169);
				if (format169)
				{
					m_aspect = 16.0 / 9;
					eAVSwitch::getInstance()->setAspectRatio(r169);
				}
				else
				{
					m_aspect = 4.0 / 3;
					eAVSwitch::getInstance()->setAspectRatio(r43);
				}
			}
			else
			if (evt.action == &i_shortcutActions->number6)
			{
				Move(50,0);
			}
			else
			if (evt.action == &i_shortcutActions->number8)
			{
				Move(0,50);
			}
			else
			if (evt.action == &i_cursorActions->down ||
			    evt.action == &i_cursorActions->right ||
			    evt.action == &i_shortcutActions->blue)
			{
				nextPicture();
				DecodeImage(*myIt, false);
				showNameOnLCD(*myIt);
				DisplayNextImage();
			}
			else
			if (evt.action == &i_cursorActions->up ||
			    evt.action == &i_cursorActions->left ||
			    evt.action == &i_shortcutActions->red)
			{
				previousPicture();
				DecodeImage(*myIt, false);
				showNameOnLCD(*myIt);
				DisplayNextImage();
			}
			break;
		}
		case eWidgetEvent::execBegin:
		{
#ifndef DISABLE_LCD
			serviceName = pLCD->lcdMain->ServiceName->getText();
#endif
			ShowImage(filename, false);
			break;
		}
		case eWidgetEvent::execDone:
		{
			fbClass::getInstance()->SetMode(screenInfo->xres, screenInfo->yres, 32);
			fbClass::getInstance()->PutCMAP();
			fbClass::getInstance()->unlock();
			if (switchto43 && format169)
				eAVSwitch::getInstance()->setAspectRatio(r43);
			showNameOnLCD(serviceName);
#ifndef DISABLE_LCD
			pLCD->lcdMain->hide();
			pLCD->lcdMenu->show();
#endif
			break;
		}
		default:
			return eWidget::eventHandler(evt);
	}
	return 1;
}

void ePictureViewer::listDirectory(eString directory, int includesubdirs)
{
	eDebug("[PICTUREVIEWER] listDirectory: dir %s", directory.c_str());
	std::list<eString> piclist;
	std::list<eString>::iterator picIt;
	std::list<eString> dirlist;
	std::list<eString>::iterator dirIt;
	piclist.clear();
	dirlist.clear();
	DIR *d = opendir(directory.c_str());
	if (d)
	{
		while (struct dirent *e = readdir(d))
		{
			eString filename = eString(e->d_name);
			eDebug("[PICTUREVIEWER] listDirectory: processing %s", filename.c_str());
			if ((filename != ".") && (filename != ".."))
			{
				struct stat s;
				eString fullFilename = directory + "/" + filename;
				if (lstat(fullFilename.c_str(), &s) < 0)
				{
					eDebug("--- file no good :(");
					continue;
				}

				if (S_ISREG(s.st_mode))
				{
					if (filename.right(4).upper() == ".JPG" ||
					    filename.right(5).upper() == ".JPEG" ||
					    filename.right(4).upper() == ".CRW" ||
					    filename.right(4).upper() == ".PNG" ||
					    filename.right(4).upper() == ".BMP" ||
					    filename.right(4).upper() == ".GIF")
					{
						eString tmp = directory + "/" + filename;
						piclist.push_back(tmp);
					}
				}
				else
				if ((includesubdirs == 1) && (S_ISDIR(s.st_mode) || S_ISLNK(s.st_mode)))
				{
					eString tmp = directory + "/" + filename;
					dirlist.push_back(tmp);
				}
			}
		}
		closedir(d);
	}
	if (!piclist.empty())
	{
		piclist.sort();
		for (picIt = piclist.begin(); picIt != piclist.end(); picIt++)
		{
			eString tmp = *picIt;
			slideshowList.push_back(tmp);
		}
	}
	if (!dirlist.empty())
	{
		dirlist.sort();
		for (dirIt = dirlist.begin(); dirIt != dirlist.end(); dirIt++)
		{
			eString tmp = *dirIt;
			listDirectory(tmp, includesubdirs);
		}
	}
}

bool ePictureViewer::DisplayNextImage()
{
	eDebug("DisplayNextImage {");
	if (m_NextPic_Buffer != NULL)
		fb_display(m_NextPic_Buffer, m_NextPic_X, m_NextPic_Y, m_NextPic_XPan, m_NextPic_YPan, m_NextPic_XPos, m_NextPic_YPos);
	eDebug("DisplayNextImage }");
	return true;
}

void ePictureViewer::Zoom(float factor)
{
	eDebug("Zoom %f {",factor);
	showBusy(m_startx + 3, m_starty + 3, 10, 0xff, 0xff, 0);

	int oldx = m_NextPic_X;
	int oldy = m_NextPic_Y;
	unsigned char *oldBuf = m_NextPic_Buffer;
	m_NextPic_X = (int)(factor * m_NextPic_X);
	m_NextPic_Y = (int)(factor * m_NextPic_Y);

	if (m_scaling == COLOR)
		m_NextPic_Buffer = color_average_resize(m_NextPic_Buffer, oldx, oldy, m_NextPic_X, m_NextPic_Y);
	else
		m_NextPic_Buffer = simple_resize(m_NextPic_Buffer, oldx, oldy, m_NextPic_X, m_NextPic_Y);

	if (m_NextPic_Buffer == oldBuf)
	{
		// resize failed
		hideBusy();
		return;
	}

	if (m_NextPic_X < (m_endx - m_startx))
		m_NextPic_XPos = (m_endx - m_startx - m_NextPic_X) / 2 + m_startx;
	else
		m_NextPic_XPos = m_startx;
	if (m_NextPic_Y < (m_endy - m_starty))
		m_NextPic_YPos = (m_endy - m_starty - m_NextPic_Y) / 2 + m_starty;
	else
		m_NextPic_YPos = m_starty;
	if (m_NextPic_X > (m_endx - m_startx))
		m_NextPic_XPan = (m_NextPic_X - (m_endx - m_startx)) / 2;
	else
		m_NextPic_XPan = 0;
	if (m_NextPic_Y > (m_endy - m_starty))
		m_NextPic_YPan = (m_NextPic_Y - (m_endy - m_starty)) / 2;
	else
		m_NextPic_YPan = 0;
	fb_display(m_NextPic_Buffer, m_NextPic_X, m_NextPic_Y, m_NextPic_XPan, m_NextPic_YPan, m_NextPic_XPos, m_NextPic_YPos);
	eDebug("Zoom }");
}
void ePictureViewer::Move(int dx, int dy)
{
	eDebug("Move %d %d {", dx, dy);
	showBusy(m_startx + 3, m_starty + 3, 10, 0, 0xff, 0);

	int xs, ys;
	getCurrentRes(&xs, &ys);
	m_NextPic_XPan += dx;
	if (m_NextPic_XPan + xs >= m_NextPic_X)
		m_NextPic_XPan = m_NextPic_X - xs - 1;
	if (m_NextPic_XPan < 0)
		m_NextPic_XPan = 0;

	m_NextPic_YPan += dy;
	if (m_NextPic_YPan + ys >= m_NextPic_Y)
		m_NextPic_YPan = m_NextPic_Y - ys - 1;
	if(m_NextPic_YPan < 0)
		m_NextPic_YPan = 0;

	if (m_NextPic_X < (m_endx - m_startx))
		m_NextPic_XPos = (m_endx - m_startx - m_NextPic_X) / 2 + m_startx;
	else
		m_NextPic_XPos = m_startx;
	if (m_NextPic_Y < (m_endy - m_starty))
		m_NextPic_YPos = (m_endy - m_starty - m_NextPic_Y) / 2 + m_starty;
	else
		m_NextPic_YPos = m_starty;
	dbout("Display x(%d) y(%d) xpan(%d) ypan(%d) xpos(%d) ypos(%d)\n",m_NextPic_X, m_NextPic_Y,
	m_NextPic_XPan, m_NextPic_YPan, m_NextPic_XPos, m_NextPic_YPos);

	fb_display(m_NextPic_Buffer, m_NextPic_X, m_NextPic_Y, m_NextPic_XPan, m_NextPic_YPan, m_NextPic_XPos, m_NextPic_YPos);
	eDebug("Move }");
}


void ePictureViewer::showBusy(int sx, int sy, int width, char r, char g, char b)
{
	eDebug("Show Busy{");

	unsigned char rgb_buffer[3];
	unsigned char *fb_buffer=0;
	unsigned char *busy_buffer_wrk;
	int cpp;
	struct fb_var_screeninfo *var = fbClass::getInstance()->getScreenInfo();

	bool yuv_fb = eSystemInfo::getInstance()->getHwType() == eSystemInfo::DM600PVR && var->bits_per_pixel == 16;

	rgb_buffer[0] = r;
	rgb_buffer[1] = g;
	rgb_buffer[2] = b;

	if (!yuv_fb)
	{
		fb_buffer = (unsigned char *) convertRGB2FB(rgb_buffer, 1, var->bits_per_pixel, &cpp);
		if (fb_buffer == NULL)
		{
			eDebug("Error: malloc");
			return;
		}
	}
	else
		cpp=2;

	if (m_busy_buffer != NULL)
	{
		delete [] m_busy_buffer;
		m_busy_buffer = NULL;
	}
	m_busy_buffer = new unsigned char[width * width * cpp];
	if (m_busy_buffer == NULL)
	{
		eDebug("Error: malloc");
		return;
	}

	busy_buffer_wrk = m_busy_buffer;
	unsigned char *fb = fbClass::getInstance()->lfb;
	unsigned int stride = fbClass::getInstance()->Stride();

	for (int y = sy ; y < sy + width; y++)
	{
		for(int x = sx ; x < sx + width; x++)
		{
			if (yuv_fb)
			{
				int offs = y * stride + x;
				busy_buffer_wrk[0]=fb[offs];  // save old Y
				busy_buffer_wrk[1]=fb[offs + var->xres * var->yres]; // save old UV
				fb[offs]=(lut_r_y[r] + lut_g_y[g] + lut_b_y[b]) >> 8;
				if (x & 1)
					fb[offs + var->xres * var->yres]=((lut_r_v[r] + lut_g_v[g] + lut_b_v[b]) >> 8) + 128;
				else
					fb[offs + var->xres * var->yres]=((lut_r_u[r] + lut_g_u[g] + lut_b_u[b]) >> 8) + 128;
			}
			else
			{
				memcpy(busy_buffer_wrk, fb + y * stride + x, cpp);
				memcpy(fb + y * stride + x * cpp, fb_buffer, cpp);
			}
			busy_buffer_wrk += cpp;
		}
	}

	m_busy_x = sx;
	m_busy_y = sy;
	m_busy_width = width;
	m_busy_cpp = cpp;
	delete [] fb_buffer;
	eDebug("Show Busy}");
}

void ePictureViewer::hideBusy()
{
	eDebug("Hide Busy {");

	if (m_busy_buffer != NULL)
	{
		unsigned char *fb = fbClass::getInstance()->lfb;
		unsigned int stride = fbClass::getInstance()->Stride();
		unsigned char *busy_buffer_wrk = m_busy_buffer;
		struct fb_var_screeninfo *var = fbClass::getInstance()->getScreenInfo();
		bool yuv_fb = eSystemInfo::getInstance()->getHwType() == eSystemInfo::DM600PVR && var->bits_per_pixel == 16;
	
		for (int y = m_busy_y; y < m_busy_y + m_busy_width; y++)
		{
			for (int x = m_busy_x; x < m_busy_x + m_busy_width; x++)
			{
				if (yuv_fb)
				{
					int offs = y * stride + x;
					fb[offs]=*(busy_buffer_wrk++);
					fb[offs + var->xres * var->yres]=*(busy_buffer_wrk++);
				}
				else
				{
					memcpy(fb + y * stride + x, busy_buffer_wrk, m_busy_cpp);
					busy_buffer_wrk += m_busy_cpp;
				}
			}
		}
		delete [] m_busy_buffer;
		m_busy_buffer = NULL;
	}
	eDebug("Hide Busy}");
}
