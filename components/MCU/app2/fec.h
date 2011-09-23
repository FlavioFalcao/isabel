/*
 * ISABEL: A group collaboration tool for the Internet
 * Copyright (C) 2009 Agora System S.A.
 * 
 * This file is part of Isabel.
 * 
 * Isabel is free software: you can redistribute it and/or modify
 * it under the terms of the Affero GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * Isabel is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * Affero GNU General Public License for more details.
 * 
 * You should have received a copy of the Affero GNU General Public License
 * along with Isabel.  If not, see <http://www.gnu.org/licenses/>.
 */
/////////////////////////////////////////////////////////////////////////
//
// $Id: fec.h 6363 2005-03-18 16:31:53Z sirvent $
//
/////////////////////////////////////////////////////////////////////////

#ifndef _MCU_FEC_H_
#define _MCU_FEC_H_

#include <rtp/RTPFec.hh>

#include "filter.h"

class fecSender_t: public filter_t
{
private:
    RTPFecSender_t *rtpFecSender;
public:

    fecSender_t(void);
    ~fecSender_t(void);

    HRESULT setK(u8 k);

    HRESULT deliver(RTPPacket_t *pkt);
};

#endif

