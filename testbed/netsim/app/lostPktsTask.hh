/////////////////////////////////////////////////////////////////////////
//
// ISABEL: A group collaboration tool for the Internet
// Copyright (C) 2009 Agora System S.A.
// 
// This file is part of Isabel.
// 
// Isabel is free software: you can redistribute it and/or modify
// it under the terms of the Affero GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// Isabel is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// Affero GNU General Public License for more details.
// 
// You should have received a copy of the Affero GNU General Public License
// along with Isabel.  If not, see <http://www.gnu.org/licenses/>.
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
// $Id: lostPktsTask.hh 20206 2010-04-08 10:55:00Z gabriel $
//
// (C) Copyright 1993-99. Dpto Ingenieria Sistemas Telematicos UPM Spain
// Dec 31, 1999 Transfered to Agora Systems S.A.
// (C) Copyright 2000-2001. Agora Systems S.A.
//
/////////////////////////////////////////////////////////////////////////

#ifndef __LOST__PKTS__TASK__
#define __LOST__PKTS__TASK__

#include "netTask.hh"

class lostPktsTask_t: public netTask_t {
    private:
       int __lostProb;
       static netsimInit_t netsimInit;

    public:
       lostPktsTask_t(int prob): __lostProb(prob) {
            debugMsg(dbg_App_Normal, "lostPktsTask_t", "PROB=%d\n", prob);
       }

       virtual void recvPkt(pktBuff_ref &pktBuff, int pktLen); 

       virtual const char *className() const {return "lostPktsTask_t";}

};

#endif
