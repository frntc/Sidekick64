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

//////////////////////////////////////////////////////////////////////////////
//                             I N C L U D E S
//////////////////////////////////////////////////////////////////////////////

#include "sidid.h"

#include <fstream>
#include <sstream>


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

void SidId::Pattern::clear()
{
    m_values.clear();
}


void SidId::Pattern::pushValue(uint_least16_t value)
{
    m_values.push_back(value);
}


bool SidId::Pattern::match(const std::vector<uint_least8_t>& buffer) const
{
    const uint_least8_t* p_buffer = &buffer.front();
    const uint_least16_t* p_values = &m_values.front();
    size_t buffer_size = buffer.size();
    size_t i = 0;
    size_t j = 0;

    while (true)
    {
        // search for first matching byte
        while ((i < buffer_size) && (p_buffer[i] != p_values[j]))
        {
            ++i;
        }
        if (i == buffer_size)
        {
            break;
        }
        size_t saved_i = i;
        size_t saved_j = j;

        // check if subsequent bytes match as well
        while ((i < buffer_size)
               && ((p_buffer[i] == p_values[j])
                   || (p_values[j] == MATCH_WILDCARD_ONE)))
        {
            ++i;
            ++j;
        }

        if (p_values[j] == MATCH_END)
        {
            // all parts matched
            break;
        }
        else if (p_values[j] == MATCH_WILDCARD_MULTIPLE)
        {
            // this part did match, continue with next part
            ++j;
        }
        else
        {
            // retry matching after first byte of current match attempt
            i = saved_i + 1;
            j = saved_j;
        }
    }

    return (p_values[j] == MATCH_END);
}


SidId::Player::Player(const std::string& name) : m_name(name)
{
    // do nothing
}


void SidId::Player::addPattern(const Pattern& pattern)
{
    m_patterns.push_back(pattern);
}


const std::string& SidId::Player::name() const
{
    return m_name;
}


    bool SidId::Player::match(const std::vector<uint_least8_t>& buffer) const
    {
        for (std::vector<Pattern>::const_iterator iter = m_patterns.begin();
             iter != m_patterns.end(); ++iter)
        {
            if (iter->match(buffer))
            {
                return true;
            }
        }
        return false;
    }


std::string SidId::trim(const std::string& str, const std::string& whitespace)
{
    const size_t begin_pos = str.find_first_not_of(whitespace);
    if (begin_pos == std::string::npos)
    {
        return "";
    }
    const size_t end_pos = str.find_last_not_of(whitespace);
    return str.substr(begin_pos, end_pos - begin_pos + 1);
}


bool SidId::readConfigFile(const std::string& filename)
{
    m_players.clear();
    std::ifstream f;
    f.open(filename.c_str());
    if (!f)
    {
        return false;
    }
    std::string line;
    while (std::getline(f, line))
    {
        line = trim(line);
        if (!line.empty())
        {
            Pattern pattern;
            std::istringstream line_ss(line);
            std::string token;
            while (line_ss >> token)
            {
                if (token == "??")
                {
                    pattern.pushValue(MATCH_WILDCARD_ONE);
                }
                else if (token == "AND")
                {
                    pattern.pushValue(MATCH_WILDCARD_MULTIPLE);
                }
                else if (token == "END")
                {
                    pattern.pushValue(MATCH_END);
                    if (m_players.empty())
                    {
                        // first player definition did not start with a name
                        return false;
                    }
                    m_players.back().addPattern(pattern);
                    pattern.clear();
                }
                else if ((token.size() == 2)
                         && isxdigit(token[0]) && isxdigit(token[1]))
                {
                    uint_least16_t i;
                    std::stringstream token_ss(token);
                    token_ss >> std::hex >> i;

                    pattern.pushValue(i);
                }
                else
                {
                    // start of new player definition
                    m_players.push_back(Player(token));
                }
            }
        }
    }

    return true;
}


std::string SidId::identify(const std::vector<uint_least8_t>& buffer)
{
    for (std::vector<Player>::const_iterator iter = m_players.begin();
         iter != m_players.end(); ++iter)
    {
        if (iter->match(buffer))
        {
            return iter->name();
        }
    }

    return "";
}
