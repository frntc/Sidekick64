/*
    $Id$

    psid64 - create a C64 executable from a PSID file
    Copyright (C) 2014  Roland Hermans <rolandh@users.sourceforge.net>

	this code has been modified for integration into the Sidekick64 software
    (the modifications are mostly removing everything that does not compile with vanilla Circle,
    I really feel sorry for having disfigured this code, please refer to the original repository 
    if you want to get the real and decent psid64 version)

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

//////////////////////////////////////////////////////////////////////////////
//                             I N C L U D E S
//////////////////////////////////////////////////////////////////////////////

#include "theme.h"


//////////////////////////////////////////////////////////////////////////////
//                           G L O B A L   D A T A
//////////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////////
//                     L O C A L   D E F I N I T I O N S
//////////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////////
//                            L O C A L   D A T A
//////////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////////
//                       L O C A L   F U N C T I O N S
//////////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////////
//                      G L O B A L   F U N C T I O N S
//////////////////////////////////////////////////////////////////////////////

DriverTheme::DriverTheme(
		int borderColor, int backgroundColor, int rasterTimeColor,
		int titleColor, const int* lineColors,
		int fieldNameColor, int fieldColonColor, int fieldValueColor,
		int legendColor,
		int progressBarFillColor, int progressBarBackgroundColor,
		int scrollerColor, const int* scrollerColors,
		const int* footerColors) :
    m_borderColor(borderColor),
    m_backgroundColor(backgroundColor),
    m_rasterTimeColor(rasterTimeColor),
    m_titleColor(titleColor),
    m_fieldNameColor(fieldNameColor),
    m_fieldColonColor(fieldColonColor),
    m_fieldValueColor(fieldValueColor),
    m_legendColor(legendColor),
    m_progressBarFillColor(progressBarFillColor),
    m_progressBarBackgroundColor(progressBarBackgroundColor),
    m_scrollerColor(scrollerColor)
{
    for (int i = 0; i < NUM_LINE_COLORS; ++i)
    {
	m_lineColors[i] = lineColors[i];
    }
    for (int i = 0; i < NUM_SCROLLER_COLORS; ++i)
    {
	m_scrollerColors[i] = scrollerColors[i];
    }
    for (int i = 0; i < NUM_FOOTER_COLORS; ++i)
    {
	m_footerColors[i] = footerColors[i];
    }
}


DriverTheme::~DriverTheme()
{
    // empty
}


DriverTheme* DriverTheme::createBlueTheme()
{
    static const int lineColors[NUM_LINE_COLORS] =
        {0x09, 0x0b, 0x08, 0x0c, 0x0a, 0x0f, 0x07, 0x01,
	 0x0d, 0x07, 0x03, 0x0c, 0x0e, 0x04, 0x06};
    static const int scrollerColors[NUM_SCROLLER_COLORS] =
        {0x05, 0x05, 0x05, 0x03, 0x0d, 0x01, 0x0d, 0x03};
    static const int footerColors[NUM_FOOTER_COLORS] =
        {0x09, 0x02, 0x04, 0x0a, 0x07, 0x0d, 0x01, 0x0d,
	 0x07, 0x0a, 0x04, 0x02, 0x09, 0x06, 0x06, 0x06};

    return new DriverTheme(
	0x06, // border color
	0x06, // background color
	0x0e, // raster time color
	0x0e, // title color
	lineColors, // line colors
	0x0d, // field name color
	0x07, // field colon color
	0x01, // field value color
	0x07, // legend color
	0x0d, // progress bar fill color
	0x01, // progress bar background color
	0x01, // scroller color
	scrollerColors, // scroller colors
	footerColors); // footer colors
}


DriverTheme* DriverTheme::createC1541UltimateTheme()
{
    static const int lineColors[NUM_LINE_COLORS] =
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0};
    static const int scrollerColors[NUM_SCROLLER_COLORS] =
        {0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f};
    static const int footerColors[NUM_FOOTER_COLORS] =
        {0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f,
	 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f};

    return new DriverTheme(
        0x00, // border color
	0x00, // background color
	0x07, // raster time color
	0x0f, // title color
	lineColors, // line colors
	0x07, // field name color
	0x07, // field colon color
	0x0f, // field value color
	0x0f, // legend color
	0x07, // progress bar fill color
	0x0f, // progress bar background color
	0x0f, // scroller color
	scrollerColors, // scroller colors
	footerColors); // footer colors
}


DriverTheme* DriverTheme::createCoalTheme()
{
    static const int lineColors[NUM_LINE_COLORS] =
        {0x0b, 0x0b, 0x0c, 0x0c, 0x0f, 0x0f, 0x01, 0x01,
	 0x01, 0x0f, 0x0f, 0x0c, 0x0c, 0x0b, 0x0b};
    static const int scrollerColors[NUM_SCROLLER_COLORS] =
        {0x0b, 0x0b, 0x0b, 0x0c, 0x0f, 1, 0x0f, 0x0c};
    static const int footerColors[NUM_FOOTER_COLORS] =
        {0x0b, 0x0b, 0x0c, 0x0c, 0x0f, 0x0f, 0x01, 0x01,
	 0x0f, 0x0f, 0x0c, 0x0c, 0x0b, 0x0b, 0x00, 0x00};

    return new DriverTheme(
	0x00, // border color
	0x00, // background color
	0x0b, // raster time color
	0x0c, // title color
	lineColors, // line colors
	0x0b, // field name color
	0x0c, // field colon color
	0x0f, // field value color
	0x0c, // legend color
	0x0b, // progress bar fill color
	0x0c, // progress bar background color
	0x0b, // scroller color
	scrollerColors, // scroller colors
	footerColors); // footer colors
}

DriverTheme* DriverTheme::createSidekickTheme()
{
    static const int lineColors[NUM_LINE_COLORS] =
//        {0x09, 0x0b, 0x08, 0x0c, 0x0a, 0x0f, 0x07, 0x01,
	// 0x0d, 0x07, 0x03, 0x0c, 0x0e, 0x04, 0x06};
        {0x0b, 0x05, 0x0d, 0x01, 0x0d, 0x05, 0x0b, 0x00,
	 0x06, 0x0e, 0x03, 0x01, 0x03, 0x0e, 0x06};
    static const int scrollerColors[NUM_SCROLLER_COLORS] =
        {0x05, 0x05, 0x05, 0x03, 0x0d, 0x01, 0x0d, 0x03};
    static const int footerColors[NUM_FOOTER_COLORS] =
       // {0x0b, 0x05, 0x0d, 0x01, 0x0d, 0x05, 0x0b, 0x00,
	 //0x06, 0x0e, 0x03, 0x01, 0x03, 0x0e, 0x06, 0x00 };
       {0x0b, 0x0b, 0x0e, 0x05, 0x03, 0x0d, 0x01, 0x01, 0x0d, 0x03, 0x05, 0x0e, 0x0b, 0x0b, 0x00, 0x00 };
	//{0x09, 0x02, 0x04, 0x0a, 0x07, 0x0d, 0x01, 0x0d,
	 //0x07, 0x0a, 0x04, 0x02, 0x09, 0x00, 0x00, 0x00};

    return new DriverTheme(
	0x0b, // border color
	0x00, // background color
	0x0c, // raster time color
	0x01, // title color
	lineColors, // line colors
	0x0d, // field name color
	0x0d, // field colon color
	0x03, // field value color
	0x0c, // legend color
	0x06, // progress bar fill color
	0x03, // progress bar background color
	0x01, // scroller color
	scrollerColors, // scroller colors
	footerColors); // footer colors
}

DriverTheme* DriverTheme::createDefaultTheme()
{
    static const int lineColors[NUM_LINE_COLORS] =
        {0x09, 0x0b, 0x08, 0x0c, 0x0a, 0x0f, 0x07, 0x01,
	 0x0d, 0x07, 0x03, 0x0c, 0x0e, 0x04, 0x06};
    static const int scrollerColors[NUM_SCROLLER_COLORS] =
        {0x05, 0x05, 0x05, 0x03, 0x0d, 0x01, 0x0d, 0x03};
    static const int footerColors[NUM_FOOTER_COLORS] =
        {0x09, 0x02, 0x04, 0x0a, 0x07, 0x0d, 0x01, 0x0d,
	 0x07, 0x0a, 0x04, 0x02, 0x09, 0x00, 0x00, 0x00};

    return new DriverTheme(
	0x00, // border color
	0x00, // background color
	0x06, // raster time color
	0x0f, // title color
	lineColors, // line colors
	0x0d, // field name color
	0x07, // field colon color
	0x01, // field value color
	0x0c, // legend color
	0x06, // progress bar fill color
	0x0e, // progress bar background color
	0x01, // scroller color
	scrollerColors, // scroller colors
	footerColors); // footer colors
}


DriverTheme* DriverTheme::createDutchTheme()
{
    static const int lineColors[NUM_LINE_COLORS] =
        {0x06, 0x06, 0x06, 0x06, 0x0e, 0x0e, 0x01, 0x01,
	 0x01, 0x0a, 0x0a, 0x02, 0x02, 0x02, 0x02};
    static const int scrollerColors[NUM_SCROLLER_COLORS] =
        {0x06, 0x06, 0x06, 0x0e, 0x03, 0x01, 0x03, 0x0e};
    static const int footerColors[NUM_FOOTER_COLORS] =
        {0x09, 0x02, 0x08, 0x0a, 0x0f, 0x07, 0x01, 0x07,
	 0x0f, 0x0a, 0x08, 0x02, 0x09, 0x00, 0x00, 0x00};

    return new DriverTheme(
	0x00, // border color
	0x00, // background color
	0x08, // raster time color
	0x0f, // title color
	lineColors, // line colors
	0x0f, // field name color
	0x0f, // field colon color
	0x01, // field value color
	0x0a, // legend color
	0x02, // progress bar fill color
	0x0b, // progress bar background color
	0x01, // scroller color
	scrollerColors, // scroller colors
	footerColors); // footer colors
}


DriverTheme* DriverTheme::createKernalTheme()
{
    static const int lineColors[NUM_LINE_COLORS] =
        {0x0e, 0x0e, 0x0e, 0x0e, 0x0e, 0x0e, 0x0e, 0x0e,
	 0x0e, 0x0e, 0x0e, 0x0e, 0x0e, 0x0e, 0x0e};
    static const int scrollerColors[NUM_SCROLLER_COLORS] =
        {0x0e, 0x0e, 0x0e, 0x0e, 0x0e, 0x0e, 0x0e, 0x0e};
    static const int footerColors[NUM_FOOTER_COLORS] =
        {0x0e, 0x0e, 0x0e, 0x0e, 0x0e, 0x0e, 0x0e, 0x0e,
	 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06};

    return new DriverTheme(
        0x0e, // border color
	0x06, // background color
	0x06, // raster time color
	0x0e, // title color
	lineColors, // line colors
	0x0e, // field name color
	0x0e, // field colon color
	0x0e, // field value color
	0x0e, // legend color
	0x0e, // progress bar fill color
	0x06, // progress bar background color
	0x0e, // scroller color
	scrollerColors, // scroller colors
	footerColors); // footer colors
}


DriverTheme* DriverTheme::createLightTheme()
{
    static const int lineColors[NUM_LINE_COLORS] =
        {0x07, 0x0f, 0x0a, 0x0c, 0x08, 0x0b, 0x09, 0x00,
	 0x06, 0x04, 0x0e, 0x0c, 0x03, 0x07, 0x0d};
    static const int scrollerColors[NUM_SCROLLER_COLORS] =
        {0x05, 0x05, 0x05, 0x03, 0x0d, 0x01, 0x0d, 0x03};
    static const int footerColors[NUM_FOOTER_COLORS] =
        {0x0d, 0x07, 0x0a, 0x04, 0x02, 0x09, 0x00, 0x09,
	 0x02, 0x04, 0x0a, 0x07, 0x0d, 0x01, 0x01, 0x01};

    return new DriverTheme(
	0x01, // border color
	0x01, // background color
	0x0d, // raster time color
	0x0b, // title color
	lineColors, // line colors
	0x05, // field name color
	0x0d, // field colon color
	0x00, // field value color
	0x0c, // legend color
	0x0e, // progress bar fill color
	0x06, // progress bar background color
	0x0b, // scroller color
	scrollerColors, // scroller colors
	footerColors); // footer colors
}


DriverTheme* DriverTheme::createMondriaanTheme()
{
    static const int lineColors[NUM_LINE_COLORS] =
        {0x06, 0x06, 0x06, 0x06, 0x0e, 0x0e, 0x07, 0x07,
	 0x07, 0x0a, 0x0a, 0x02, 0x02, 0x02, 0x02};
    static const int scrollerColors[NUM_SCROLLER_COLORS] =
        {0x02, 0x02, 0x02, 0x0a, 0x0f, 0x01, 0x0f, 0x0a};
    static const int footerColors[NUM_FOOTER_COLORS] =
        {0x02, 0x02, 0x0a, 0x0a, 0x07, 0x07, 0x01, 0x01,
	 0x07, 0x07, 0x0e, 0x0e, 0x06, 0x06, 0x00, 0x00};

    return new DriverTheme(
	0x00, // border color
	0x00, // background color
	0x02, // raster time color
	0x0f, // title color
	lineColors, // line colors
	0x0c, // field name color
	0x0e, // field colon color
	0x0f, // field value color
	0x07, // legend color
	0x06, // progress bar fill color
	0x0e, // progress bar background color
	0x01, // scroller color
	scrollerColors, // scroller colors
	footerColors); // footer colors
}


DriverTheme* DriverTheme::createOceanTheme()
{
    static const int lineColors[NUM_LINE_COLORS] =
        {0x0d, 0x03, 0x0c, 0x0e, 0x04, 0x06, 0x06, 0x06,
	 0x06, 0x06, 0x04, 0x0e, 0x0c, 0x03, 0x0d};
    static const int scrollerColors[NUM_SCROLLER_COLORS] =
        {0x03, 0x03, 0x03, 0x0e, 0x06, 0x00, 0x06, 0x0e};
    static const int footerColors[NUM_FOOTER_COLORS] =
        {0x0f, 0x0e, 0x04, 0x06, 0x06, 0x06, 0x06, 0x06,
	 0x06, 0x06, 0x06, 0x06, 0x04, 0x0e, 0x0f, 0x01};

    return new DriverTheme(
	0x01, // border color
	0x01, // background color
	0x0e, // raster time color
	0x06, // title color
	lineColors, // line colors
	0x03, // field name color
	0x0e, // field colon color
	0x06, // field value color
	0x0e, // legend color
	0x06, // progress bar fill color
	0x0e, // progress bar background color
	0x0b, // scroller color
	scrollerColors, // scroller colors
	footerColors); // footer colors
}


DriverTheme* DriverTheme::createPencilTheme()
{
    static const int lineColors[NUM_LINE_COLORS] =
        {0x0f, 0x0c, 0x0b, 0x00, 0x00, 0x00, 0x00, 0x00,
	 0x00, 0x00, 0x00, 0x00, 0x0b, 0x0c, 0x0f};
    static const int scrollerColors[NUM_SCROLLER_COLORS] =
        {0x0f, 0x0f, 0x0f, 0x0c, 0x0b, 0x00, 0x0b, 0x0c};
    static const int footerColors[NUM_FOOTER_COLORS] =
        {0x0f, 0x0c, 0x0b, 0x00, 0x00, 0x00, 0x00, 0x00,
	 0x00, 0x00, 0x00, 0x00, 0x0b, 0x0c, 0x0f, 0x01};

    return new DriverTheme(
	0x01, // border color
	0x01, // background color
	0x0f, // raster time color
	0x00, // title color
	lineColors, // line colors
	0x0c, // field name color
	0x0f, // field colon color
	0x0b, // field value color
	0x0c, // legend color
	0x0b, // progress bar fill color
	0x0c, // progress bar background color
	0x0b, // scroller color
	scrollerColors, // scroller colors
	footerColors); // footer colors
}


DriverTheme* DriverTheme::createRainbowTheme()
{
    static const int lineColors[NUM_LINE_COLORS] =
        {0x06, 0x06, 0x06, 0x05, 0x05, 0x05, 0x07, 0x07,
	 0x07, 0x08, 0x08, 0x08, 0x02, 0x02, 0x02};
    static const int scrollerColors[NUM_SCROLLER_COLORS] =
        {0x02, 0x08, 0x07, 0x05, 0x06, 0x00, 0x00, 0x00};
    static const int footerColors[NUM_FOOTER_COLORS] =
        {0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f,
	 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f};

    return new DriverTheme(
	0x00, // border color
	0x00, // background color
	0x0b, // raster time color
	0x0f, // title color
	lineColors, // line colors
	0x0f, // field name color
	0x0c, // field colon color
	0x01, // field value color
	0x0c, // legend color
	0x0b, // progress bar fill color
	0x0f, // progress bar background color
	0x01, // scroller color
	scrollerColors, // scroller colors
	footerColors); // footer colors
}
