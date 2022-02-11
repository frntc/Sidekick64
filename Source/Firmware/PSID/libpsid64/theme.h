/*
    $Id$

    psid64 - create a C64 executable from a PSID file
    Copyright (C) 2014  Roland Hermans <rolandh@users.sourceforge.net>

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

#ifndef THEME_H
#define THEME_H

//////////////////////////////////////////////////////////////////////////////
//                             I N C L U D E S
//////////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////////
//                  F O R W A R D   D E C L A R A T O R S
//////////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////////
//                     D A T A   D E C L A R A T O R S
//////////////////////////////////////////////////////////////////////////////

#define NUM_LINE_COLORS			15
#define NUM_SCROLLER_COLORS		8
#define NUM_FOOTER_COLORS		16

class DriverTheme
{
public:
    DriverTheme(int borderColor, int backgroundColor, int rasterTimeColor,
                int titleColor, const int* lineColors,
		int fieldNameColor, int fieldColonColor, int fieldValueColor,
		int legendColor,
		int progressBarFillColor, int progressBarBackgroundColor,
		int scrollerColor, const int* scrollerColors,
		const int* footerColors);
    virtual ~DriverTheme();

    // factory methods
    static DriverTheme* createBlueTheme();
    static DriverTheme* createC1541UltimateTheme();
    static DriverTheme* createCoalTheme();
    static DriverTheme* createDefaultTheme();
    static DriverTheme* createSidekickTheme();
    static DriverTheme* createDutchTheme();
    static DriverTheme* createKernalTheme();
    static DriverTheme* createLightTheme();
    static DriverTheme* createMondriaanTheme();
    static DriverTheme* createOceanTheme();
    static DriverTheme* createPencilTheme();
    static DriverTheme* createRainbowTheme();

    int getBorderColor() const
    {
	return m_borderColor;
    }

    int getBackgroundColor() const
    {
	return m_backgroundColor;
    }

    int getRasterTimeColor() const
    {
	return m_rasterTimeColor;
    }

    int getTitleColor() const
    {
	return m_titleColor;
    }

    const int* getLineColors() const
    {
	return m_lineColors;
    }

    int getFieldNameColor() const
    {
	return m_fieldNameColor;
    }

    int getFieldColonColor() const
    {
	return m_fieldColonColor;
    }

    int getFieldValueColor() const
    {
	return m_fieldValueColor;
    }

    int getLegendColor() const
    {
	return m_legendColor;
    }

    int getProgressBarFillColor() const
    {
	return m_progressBarFillColor;
    }

    int getProgressBarBackgroundColor() const
    {
	return m_progressBarBackgroundColor;
    }

    int getScrollerColor() const
    {
	return m_scrollerColor;
    }

    const int* getScrollerColors() const
    {
	return m_scrollerColors;
    }

    const int* getFooterColors() const
    {
	return m_footerColors;
    }

private:
    int m_borderColor;
    int m_backgroundColor;
    int m_rasterTimeColor;
    int m_titleColor;
    int m_lineColors[NUM_LINE_COLORS];
    int m_fieldNameColor;
    int m_fieldColonColor;
    int m_fieldValueColor;
    int m_legendColor;
    int m_progressBarFillColor;
    int m_progressBarBackgroundColor;
    int m_scrollerColor;
    int m_scrollerColors[NUM_SCROLLER_COLORS];
    int m_footerColors[NUM_FOOTER_COLORS];
};

#endif // THEME_H
