/*
    $Id$

    psid64 - create a C64 executable from a PSID file
    Copyright (C) 2015  Roland Hermans <rolandh@users.sourceforge.net>

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

#ifndef SIDID_H
#define SIDID_H

//////////////////////////////////////////////////////////////////////////////
//                             I N C L U D E S
//////////////////////////////////////////////////////////////////////////////

#include <string>
#include <vector>

#include "../sidplay/sidint.h"


//////////////////////////////////////////////////////////////////////////////
//                  F O R W A R D   D E C L A R A T O R S
//////////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////////
//                     D A T A   D E C L A R A T O R S
//////////////////////////////////////////////////////////////////////////////

class SidId
{
    class Pattern
    {
        std::vector<uint_least16_t> m_values;
    public:
        void clear();
        void pushValue(uint_least16_t value);
        bool match(const std::vector<uint_least8_t>& buffer) const;
    };

    class Player
    {
        std::string m_name;
        std::vector<Pattern> m_patterns;
    public:
        explicit Player(const std::string& name);
        void addPattern(const Pattern& pattern);
        const std::string& name() const;
        bool match(const std::vector<uint_least8_t>& buffer) const;
    };

    std::vector<Player> m_players;

    static const uint_least16_t MATCH_WILDCARD_ONE = 0x100;
    static const uint_least16_t MATCH_WILDCARD_MULTIPLE = 0x101;
    static const uint_least16_t MATCH_END = 0x102;

    static std::string trim(const std::string& str,
                            const std::string& whitespace = " \t\n\r");
public:
    bool readConfigFile(const std::string& filename);
    std::string identify(const std::vector<uint_least8_t>& buffer);
};

#endif // SIDID_H
