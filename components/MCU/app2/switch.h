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
// $Id: switch.h 6859 2005-06-14 09:57:20Z sirvent $
//
/////////////////////////////////////////////////////////////////////////

#ifndef _MCU_SWITCH_H_
#define _MCU_SWITCH_H_

#include "flowProcessor.h"

class switchProcessor_t: public processor_t
{
private:
    // every processor_t must save usefull
    // info to check if the processor is valid
    // to be used by other inFlows
    u8 PT;
    u32 SSRC;

public:
    switchProcessor_t(u8 PT,u32 SSRC);
    virtual ~switchProcessor_t(void);

    u8 getPT(void);
    u32 getSSRC(void) {return SSRC;}

    virtual HRESULT deliver(RTPPacket_t *pkt);

    // to check if we've got a valid
    // processor type
    virtual bool operator==(switchProcessor_t *);
    virtual bool operator==(switchProcessor_t);

};

class switcher_t: public flowProcessor_t
{
private:

     // to create new processors
     switchProcessor_t *getValidProcessor(flow_t inFlow,
                                          vector_t<outFlow_t *> *outFlowArray,
                                          u32 SSRC=0
                                         );
     switchProcessor_t *getValidProcessor(flow_t inFlow, u32 SSRC=0);

public:

    switcher_t(void);
    virtual ~switcher_t(void);

    // to define inFlow-outFlow relationShip
    virtual HRESULT setFlow(flow_t inFlow,
                            target_t *target,
                            u8 PT=0,
                            u32 BW=0,
                            u16 width=0,
                            u16 height=0,
                            u32 FR = 0,
                            u8  Q = 0,
                            u32 SSRC = 0,
                            gridMode_e gridMode= GRID_AUTO
                           );

    virtual HRESULT unsetFlow(flow_t inFlow, target_t *target);

    virtual HRESULT deliver(RTPPacket_t *pkt, flow_t inFlow);
};

#endif

