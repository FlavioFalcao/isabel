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
// $Id: linkProtocol.cc 20759 2010-07-05 10:30:36Z gabriel $
//
/////////////////////////////////////////////////////////////////////////

#include <icf2/general.h>
#include <icf2/notify.hh>

#ifdef __BUILD_FOR_LINUX
#include <netdb.h>
#endif

#include "linkProtocol.hh"
#include "webirouterConfig.hh"


//
// global harbinger // chapuza santi & jsr & trobles
//
linkHarbinger_t *theHarbinger= NULL;
//
// SSM
//
mcastLinkHarbinger_t *theMcastHarbinger= NULL;


bool is_ok_irouter_version(u16 iMajorVer, u16 iMinorVer);


//---------------------------------------------------
//
// linkOnDemandTask_t
//    espera conexiones desde otros irouters
//    actualizacion de estado blando, los datos
//    no refrescados se borran.
//
//---------------------------------------------------



linkOnDemandTask_t::linkOnDemandTask_t(transApp_t *app,
                                       linkControl_t *mgr,
                                       const inetAddr_t &remote,
                                       long bw,
                                       bool echoBool,
                                       int n,
                                       int k,
                                       bool type
                                      )
: simpleTask_t(8*LM_TIMEOUT*100000),
  myApp(app),
  manager(mgr),
  stillActive(true),
  host((char*)malloc(256)),
  hostAddr(remote),
  nominalBandWidth(bw)
{
    this->type = type;
    memset(host,0,256);

    debugMsg(dbg_App_Normal,
             "linkOnDemandTask_t",
             "demanding=[%s] BW=%d",
             hostAddr.getStrValue(),
             bw
            );

    sprintf(host, "%s", hostAddr.getStrValue());
    char *linkName = host;

    link= app->linkBinder->newLink(linkName,
                                   link_t::inputOutput,
                                   echoBool,
                                   bw,
                                   type
                                  );

    app->add_target_for_all_flows(linkName, linkName, n, k);
}

linkOnDemandTask_t::linkOnDemandTask_t(linkOnDemandTask_t &other)
{
    myApp       = other.myApp;
    manager     = other.manager;
    stillActive = other.stillActive;

    if (host)
    {
        free(host);
        host = NULL;
    }
    if (other.host != NULL)
    {
        host = strdup(other.host);
    }

    hostAddr= other.hostAddr;
    link    = other.link;
}

linkOnDemandTask_t &
linkOnDemandTask_t::operator=(linkOnDemandTask_t &other)
{
    myApp       = other.myApp;
    manager     = other.manager;
    stillActive = other.stillActive;

    if (host)
    {
        free(host);
        host = NULL;
    }
    if (other.host != NULL)
    {
        host = strdup(other.host);
    }

    hostAddr= other.hostAddr;
    link    = other.link;
    return *this;
}

linkOnDemandTask_t::~linkOnDemandTask_t(void)
{
    debugMsg(dbg_App_Normal,
             "~linkOnDemandTask_t",
             "Destroying linkOnDemandTask_t host=[%s]\n",
             hostAddr.getStrValue()
            );

    char *linkName=host;
    myApp->delete_link(linkName);

    if (host) {
        free(host);
        host = NULL;
    }

    debugMsg(dbg_App_Paranoic,
             "~linkOnDemandTask_t",
             "Destroyed linkOnDemandTask_t\n"
            );

    NOTIFY("Disconnecting host [%s]\n", hostAddr.getStrValue());
    NOTIFY("Destroying linkOnDemandTask_t [%s]\n", hostAddr.getStrValue());
}

void
linkOnDemandTask_t::heartBeat(void)
{
    if ( ! stillActive)
    {
        debugMsg(dbg_App_Normal,
                 "heartBeat",
                 "link-%s: removing this on demand link [%s]",
                 host,
                 hostAddr.getStrValue()
                );

        NOTIFY("link-%s: removing this on demand link [%s]\n",
               host,
               hostAddr.getStrValue()
              );

        //
        // SSM
        //
        if (myApp->irouterParam->getSSM())
        {
            deleteMulticastSources(hostAddr.getStrValue());
        }

        // Si no lo hago, nunca se va a llamar al destructor
        // porque va a quedar esta referencia sujentado al objeto
        if (manager)
        {
            //string address = string(hostAddr.getStrValue());
            //manager->remotesByAddress.remove(address);
            manager->remotesByAddress.remove(hostAddr.getStrValue());
        }

        get_owner()->removeTask(this);

    }
    else
    {
        debugMsg(dbg_App_Normal,
                 "heartBeat",
                 "link-%s: survived one more time [%s]",
                 host,
                 hostAddr.getStrValue()
                );

        stillActive= false;
    }
}

void
linkOnDemandTask_t::adjustBandWidth(u32 bw)
{
    nominalBandWidth= bw;
    //link->adjustBandWidth(bw);
}

//
// SSM
//
void
linkOnDemandTask_t::deleteMulticastSources(const char *buffer)
{
    stringArray_ref sources;

    sources= myApp->irouterParam->getSources();

    for (int i = 0; i < sources->size(); i++)
    {
        char *aux = sources->elementAt(i);
        if (strcmp(buffer, aux) == 0)
        {
            // Leave
            myApp->leaveSSM(aux);

            debugMsg(dbg_App_Normal,
                     "DeleteMulticastSources",
                     "Source [%s] deleted",
                     aux
                    );

            sources->remove(i);
            i--;
        }
    }

    myApp->irouterParam->setMulticastSources(sources);
}

//---------------------------------------------------
//
// linkControl_t
//    atencion de peticion de conexion de otros FS
//
//---------------------------------------------------


linkControl_t::linkControl_t(transApp_t *app, bool nacceptConnections)
: remoteUniqueId(0),
  acceptConnections(nacceptConnections),
  myApp(app),
  remotesByAddress(64)
{
    int iLinkProtocol = app->irouterParam->getCtrlPort();

    memset(irouterLinkPort, 0, 16);
    sprintf(irouterLinkPort, "%d", iLinkProtocol);

    internalSock = myApp->targetMgr->sockPool->getSock(NULL, iLinkProtocol);

    debugMsg(dbg_App_Paranoic,
             "linkControl_t",
             "Adding new linkControl_t internalSock->sysHandle()=%d\n",
             internalSock->sysHandle()
            );

    io_ref io = static_cast<tgtSock_t *>(internalSock);
    add_IO(io);

    flowServerTgt = NULL;

    //
    // SSM
    //
    mcastServerTgt = NULL;
    mcastLeader=myApp->irouterParam->getMcastLeader();
    if (mcastLeader)
    {
        // If it is the father, its address is the first in the list
        refreshMulticastSources(leaderIpMsg());
    }

    debugMsg( dbg_App_Verbose, "linkControl_t", "link control manager built");
}

linkControl_t::~linkControl_t(void)
{
    // Si no ponemos este puntero a NULL, al destruir los
    // linkOnDemandTask_ref  va a intentar acceder a
    // este objeto que ya ha desaparecido!!!
    debugMsg(dbg_App_Paranoic, "~linkControl_t", "Destroying linkControl\n");

    ql_t<string> *keys = remotesByAddress.getKeys();
    for (ql_t<string>::iterator_t  i = keys->begin();
         i != keys->end();
         i++
        )
    {
        string key = i;
        remotesByAddress.lookUp(key)->manager = NULL;
    }
    delete keys;

    if (flowServerTgt)
    {
        free(flowServerTgt);
    }

    //
    //  SSM
    //
    if (mcastServerTgt)
    {
        free(mcastServerTgt);
    }

    debugMsg(dbg_App_Paranoic, "~linkControl_t", "Destroyed linkControl\n");
}


void
linkControl_t::IOReady(io_ref &io)
{
    linkMsg_t itsYou;

    inetAddr_t addr(NULL, irouterLinkPort, serviceAddr_t::DGRAM);
    io_t *io_p = io;
    dgramSocket_t *iioo= static_cast<dgramSocket_t *>(io_p);

    int length = iioo->recvFrom(addr, &itsYou, sizeof(itsYou));

    if (length <= 0)
    {
        NOTIFY("linkControl_t::IOReady::FD_ISSET but can't read\n");
        return;
    }

    debugMsg(dbg_App_Normal,
             "IOReady",
             "Message from Harbinger to update soft state"
            );

    msgID_e msgID = (msgID_e)ntohl(itsYou.msgType);

    //
    // SSM Nos aseguramos de que es el padre el que esta recibiendo
    // los mcastRequest
    //
    bool acceptMcastConnections=myApp->irouterParam->getMcastLeader();

    if ( ! acceptMcastConnections && (msgID == mcastLinkReq))
    {
        NOTIFY("linkControl_t::IOReady: this irouter has been configured "
               "to accept no connections from other Multicast\n"
              );

        return;
    }


    if ( ! acceptConnections && (msgID == linkReq))
    {
        NOTIFY("linkControl_t::IOReady: this irouter has been configured "
               "to accept no connections from other FS\n"
              );

        return;
    }

    // Check Irouter protocol version

    if (!is_ok_irouter_version(ntohs(itsYou.iMajorVer),
                               ntohs(itsYou.iMinorVer)))
    {
        // Error version:: Response to the connection request
        NOTIFY("IOReady:: Could not connect to [%s] version error\n",
               addr.getStrValue()
              );

        if (msgID == versionErr)
        {
            NOTIFY("IOReady:: different versions\n");
            NOTIFY("exiting...\n");

            exit(-1); // soy el que solicita la conexion,
                      // y el FS padre tiene otra version
        }

        sendErrorVersionPkt(iioo, addr, length);

        return;
    }


    // Process packet

    switch (msgID)
    {
    case linkReq:
        processConnectionRequest(iioo, addr, itsYou, length);
        break;

    case linkAns:
         processConnectionAnswer(itsYou, addr, length);
         break;

//
// SSM Los dos mensajes multicast
//
    case mcastLinkReq:
        processMcastConnectionRequest(iioo, addr, itsYou, length);
        break;

    case mcastLinkAns:
         processMcastConnectionAnswer(itsYou, addr, length);
         break;

    default:
        NOTIFY("linkControl_t::IOReady: Unknown message type...\n");
    }
}

void
linkControl_t::sendErrorVersionPkt(dgramSocket_t *iioo,
                                   inetAddr_t &addr,
                                   int length
                                  )
{
    linkMsg_t itsMe;

    itsMe.msgType         = (msgID_e)htonl(versionErr);
    itsMe.iMajorVer       = htons(IROUTER_MAJOR_VERSION);
    itsMe.iMinorVer       = htons(IROUTER_MINOR_VERSION);
    itsMe.linkBandWidth   = 0; // no es necesario
    itsMe.linkEchoRequired= htonl(0);

    inetAddr_t remoteAddr(addr.getStrValue(),
                          irouterLinkPort,
                          serviceAddr_t::DGRAM
                         );
    iioo->writeTo(addr, &itsMe, sizeof(itsMe));
}

void
linkControl_t::processConnectionRequest(dgramSocket_t *iioo,
                                        inetAddr_t &addr,
                                        linkMsg_t &itsYou,
                                        int length
                                       )
{
    // Connection request from a leaf flowserver
    unsigned bw = ntohl(itsYou.linkBandWidth);

    debugMsg(dbg_App_Normal,
             "processConnectionRequest",
             "mylocalid=%d got: bw=%lu echo=%s",
             localNodeIdentifier,
             bw,
             ntohl(itsYou.linkEchoRequired)?"yes":"no"
            );

    linkOnDemandTask_ref lnkOD = remotesByAddress.lookUp(addr.getStrValue());
    if (lnkOD.isValid())
    {
        debugMsg(dbg_App_Normal,
                 "processConnectionRequest",
                 "rejuvenating linkOnDemandTask_t [%s]\n",
                 addr.getStrValue()
                );

        if (lnkOD->nominalBandWidth != bw)
        {
            debugMsg(dbg_App_Normal,
                     "processConnectionRequest",
                     "changing bandwidth to %ld",
                     bw
                    );

            lnkOD->adjustBandWidth(bw);
        }

        lnkOD->stillActive= true;

    }
    else
    {
        debugMsg(dbg_App_Normal,
                 "processConnectionRequest",
                 "building linkOnDemandTask_t [%s]\n",
                 addr.getStrValue()
                );

        NOTIFY("linkControl_t:: accepting FS connection request from [%s]\n"
               "linkControl_t:: requesting BW=%d\n"
               "linkControl_t:: building linkOnDemandTask_t\n",
               addr.getStrValue(),
               bw
              );

        lnkOD= new linkOnDemandTask_t(myApp,
                                      this,
                                      addr,
                                      bw,
                                      ntohl(itsYou.linkEchoRequired),
                                      ntohs(itsYou.n),
                                      ntohs(itsYou.k)
                                     );

        remotesByAddress.insert(addr.getStrValue(), lnkOD);

        myApp->insertTask(static_cast<simpleTask_t *>(lnkOD));
    }

    // Response to the connection request
    linkMsg_t itsMe;

    itsMe.msgType         = (msgID_e)htonl(linkAns);
    itsMe.iMajorVer       = htons(IROUTER_MAJOR_VERSION);
    itsMe.iMinorVer       = htons(IROUTER_MINOR_VERSION);
    itsMe.linkBandWidth   = 0; // no es necesario
    itsMe.linkEchoRequired= htonl(0);
    itsMe.n               = itsYou.n;
    itsMe.k               = itsYou.k;

    inetAddr_t remoteAddr(addr.getStrValue(),
                          irouterLinkPort,
                          serviceAddr_t::DGRAM
                         );

    iioo->writeTo(addr, &itsMe, sizeof(itsMe));
}

void
linkControl_t::processConnectionAnswer(linkMsg_t &itsYou,
                                       inetAddr_t &addr,
                                       int length
                                      )
{
   // Make irouter answer addr test only
   // if we can resolve the flowServerTgt
   struct hostent *h= gethostbyname(myApp->irouterParam->getFlowServerTgt());
   if (h)
   {
        if (strstr(addr.getStrValue(),
            myApp->irouterParam->getFlowServerTgt()) == NULL)
        {
            NOTIFY("linkHarbinger_t:: Unknown FS is responding "
                     "to the connection request:: "
                     "Unknown Irouter=%s :: Expected Irouter=%s\n",
                     addr.getStrValue(),
                     myApp->irouterParam->getFlowServerTgt()
                    );

            //assert(false &&
            //       "Unknown FS is responding to the connection request!!"
            //      );
            return;
        }
    }
    else
    {
         if (!flowServerTgt)
         {
             NOTIFY("LinkProtocol :: Can't resolve FlowServerIP [%s], "
                    "accepting answer from [%s]\n",
                    myApp->irouterParam->getFlowServerTgt(),
                    addr.getStrValue()
                   );
         }
    }

    if ( ! flowServerTgt)
    {
        flowServerTgt = strdup(addr.getStrValue());

        debugMsg(dbg_App_Normal,
                 "processConnectionAnswer",
                 "annadiendo target a =%s\n",
                 flowServerTgt
                );

        char *linkName = flowServerTgt;
        myApp->linkBinder->newLink(linkName,
                                   link_t::inputOutput,
                                   false,
                                   0
                                  );

        myApp->add_target_for_all_flows(linkName,
                                        linkName,
                                        ntohs(itsYou.n),
                                        ntohs(itsYou.k)
                                       );

        NOTIFY("linkHarbinger_t:: Answer from expected flowServer [%s] \n",
               addr.getStrValue()
              );
    }
}

//
// SSM New functions to manage SSM messages.
//
char *
linkControl_t::leaderIpMsg(void)
{
    char *almacen = new char[512];
    memset(almacen,0,512);
    char *tmp = myApp->irouterParam->getmyip();

    strcpy(almacen,tmp);
    strcat(almacen,"W");
    strcat(almacen,"W");

    return almacen;
}

char *
linkControl_t::getMulticastSources(void)
{
    // Genera un mcastMsg con la lista de fuentes
    char *fuentes = new char[512];
    memset(fuentes, 0, 512);

    stringArray_ref sources;
    sources= myApp->irouterParam->getSources();

    // Recorro mi lista de fuentes
    for (int i = 0; i < sources->size(); i++)
    {
        char *aux = sources->elementAt(i);
        strcat(fuentes, aux);
        strcat(fuentes, "W");
    }
    strcat(fuentes, "W");

    return fuentes;
}

void
linkControl_t::refreshMulticastSources(char *buffer)
{
    // Actualiza mi lista de fuentes
    stringArray_ref recepMulticast;
    int newSource;

    char *cadenaRec= new char[512];
    memset(cadenaRec, 0, 512);
    strcpy(cadenaRec, buffer);

    stringArray_ref sources;
    sources= myApp->irouterParam->getSources();

    if ( ! myApp->irouterParam->getMcastLeader())
    {
        // if son test if any source has been deleted
        for (int i = 0; i < sources->size(); i++)
        {
            char *source = sources->elementAt(i);
            if (strstr(cadenaRec, source) == NULL &&
                strcmp(source, myApp->irouterParam->getmyip()) != 0
               ) // deleted source!
            {
                NOTIFY("linkControl_t::refreshMulticastSources: "
                       "leaving source %s\n",
                       source
                      );

                sources->remove(i);
                myApp->leaveSSM(source);
                i--;
            }
        }
    }

    char *token;
    token = strtok(cadenaRec, "W");

    while (token != NULL)
    {
        recepMulticast->add(token); // Guardo una lista de las IPs recibidas
        token = strtok(NULL, "W");
    }

    // Recorro la lista recibida
    for (int i = 0; i < recepMulticast->size(); i++)
    {
        newSource= 0;
        char *aux = recepMulticast->elementAt(i);
        for (int j = 0; j < sources->size(); j++)
        {
            char *list= sources->elementAt(j);
            if (strcmp(list, aux) == 0)
            {
                // Si es igual es porque ya tengo esa IP guardada
                newSource= 1;
            }
        }
        if (newSource == 0)
        {
            sources->add(aux);
            // Join
            if (strcmp(myApp->irouterParam->getmyip(), aux) != 0)
            {
                NOTIFY("linkControl_t::refreshMulticastSources:: "
                       "joining source %s\n",
                       aux
                      );
                myApp->joinSSM(aux);
            }
        }
    }
    myApp->irouterParam->setMulticastSources(sources);
}
//
// SSM. Answering SSM messages
//

void
linkControl_t::processMcastConnectionRequest(dgramSocket_t *iioo,
                                             inetAddr_t &addr,
                                             linkMsg_t &itsYou,
                                             int length
                                            )
{
    // Connection request from a leaf multicast
    unsigned bw = ntohl(itsYou.linkBandWidth);

    debugMsg(dbg_App_Normal,
             "processMcastConnectionRequest",
             "mylocalid=%d got: bw=%lu echo=%s",
             localNodeIdentifier,
             bw,
             ntohl(itsYou.linkEchoRequired)?"yes":"no"
            );

    linkOnDemandTask_ref lnkOD = remotesByAddress.lookUp(addr.getStrValue());
    if (lnkOD.isValid())
    {
        debugMsg(dbg_App_Normal,
                 "processMcastConnectionRequest",
                 "rejuvenating linkOnDemandTask_t [%s]\n",
                 addr.getStrValue()
                );

        if (lnkOD->nominalBandWidth != bw)
        {
            debugMsg(dbg_App_Normal,
                     "processMcastConnectionRequest",
                     "changing bandwidth to %ld",
                     bw
                    );

            lnkOD->adjustBandWidth(bw);
        }
        lnkOD->stillActive= true;

    }
    else
    {
        debugMsg(dbg_App_Normal,
                 "processMcastConnectionRequest",
                 "building linkOnDemandTask_t [%s]\n",
                 addr.getStrValue()
                );

        NOTIFY("linkControl_t:: Accepting McastServer connection request"
               " from [%s] BW=%d\n"
               "linkControl_t:: building linkOnDemandTask_t\n",
               addr.getStrValue(),
               bw
              );

        lnkOD= new linkOnDemandTask_t(myApp,
                                      this,
                                      addr,
                                      bw,
                                      ntohl(itsYou.linkEchoRequired),
                                      ntohs(itsYou.n),
                                      ntohs(itsYou.k),
                                      CONTROL
                                     );

        remotesByAddress.insert(addr.getStrValue(), lnkOD);

        myApp->insertTask(static_cast<simpleTask_t *>(lnkOD));
    }

    // Response to the connection request

    linkMsg_t itsMe;

    itsMe.msgType         = (msgID_e)htonl(mcastLinkAns);
    itsMe.iMajorVer       = htons(IROUTER_MAJOR_VERSION);
    itsMe.iMinorVer       = htons(IROUTER_MINOR_VERSION);
    itsMe.linkBandWidth   = 0; // no es necesario
    itsMe.linkEchoRequired= htonl(0);
    itsMe.n               = itsYou.n;
    itsMe.k               = itsYou.k;
//
// SSM
//

    debugMsg(dbg_App_Paranoic,
            "Mcast Request",
            "Receiving request from multicast SSM:[%s]",
            itsYou.multicastMsg
            );
    memset(itsMe.multicastMsg,0,512);

    // Actualizo la lista de fuentes validas
    refreshMulticastSources(itsYou.multicastMsg);
    char *fuentes= getMulticastSources();
    strncpy(itsMe.multicastMsg, fuentes, strlen(fuentes));

    debugMsg(dbg_App_Paranoic,
             "Mcast Request",
             "Sending message %s to refresh SSM sources",
             itsMe.multicastMsg
            );
    delete[] fuentes;

    inetAddr_t remoteAddr(addr.getStrValue(),
                          irouterLinkPort,
                          serviceAddr_t::DGRAM
                         );
    iioo->writeTo(addr, &itsMe, sizeof(itsMe));
}

void
linkControl_t::processMcastConnectionAnswer(linkMsg_t &itsYou,
                                            inetAddr_t &addr,
                                            int length
                                           )
{
    debugMsg(dbg_App_Paranoic,
             "Mcast Answer",
             "I should update my SSM sources list: %s",
             itsYou.multicastMsg
            );

    // Received answer from SSM father. Updating SSM sources list.
    refreshMulticastSources(itsYou.multicastMsg);

    if (strstr(addr.getStrValue(), myApp->irouterParam->getMcastServerTgt())<0)
    {
        NOTIFY("linkHarbinger_t::Unknown Mcast is responding to "
               "the connection request::"
               "Unknown Irouter=%s :: Expected Irouter=%s\n",
               addr.getStrValue(),
               myApp->irouterParam->getMcastServerTgt()
              );
        //assert(false &&
        //       "Unknown Mcast is responding to the connection request!!"
        //      );
        return;
    }

    if ( ! mcastServerTgt)
    {
       mcastServerTgt = strdup(addr.getStrValue());

       debugMsg(dbg_App_Normal,
                "processMcastConnectionAnswer",
                "adding target to =%s\n",
                mcastServerTgt
               );

        char *linkName = mcastServerTgt;

        myApp->linkBinder->newLink(linkName,
                                   link_t::inputOutput,
                                   false,
                                   0,
                                   CONTROL
                                  );

        myApp->add_target_for_all_flows(linkName,
                                        linkName,
                                        ntohs(itsYou.n),
                                        ntohs(itsYou.k)
                                       );

        NOTIFY("linkHarbinger_t:: Answer from expected mcastServer [%s]\n",
               mcastServerTgt,myApp->irouterParam->getMcastServerTgt()
              );
    }
}


//---------------------------------------------------
//
// linkHarbinger_t
//    envio de peticion de conexion
//    envio periodico para refresco de estado blando
//
//---------------------------------------------------


linkHarbinger_t::linkHarbinger_t(transApp_t *app,
                                 const char *remote,
                                 int bw,
                                 bool echo,
                                 int new_n,
                                 int new_k
                                )
: simpleTask_t(/*LM_TIMEOUT*/2*1000000),
  bandWidth(bw),
  echoMode(echo),
  remoteUniqueId(0),
  n(new_n),
  k(new_k),
  myApp(app)
{
    NOTIFY("---------------------------------\n"
           "Connecting to the flowserver [%s]\n"
           "---------------------------------\n",
           remote
          );

    int iLinkPort = app->irouterParam->getCtrlPort();
    char myPort[16];
    memset(myPort, 0, 16);
    sprintf(myPort, "%d", iLinkPort);
    internalSock = myApp->targetMgr->sockPool->getSock(NULL, iLinkPort);
    remoteAddr = new inetAddr_t(remote, myPort, serviceAddr_t::DGRAM);

    debugMsg(dbg_App_Verbose, "linkHarbinger_t", "link harbinger built");
}

void
linkHarbinger_t::heartBeat(void)
{
    linkMsg_t   itsMe;

    itsMe.iMajorVer       = htons(IROUTER_MAJOR_VERSION);
    itsMe.iMinorVer       = htons(IROUTER_MINOR_VERSION);
    itsMe.msgType         = (msgID_e)htonl(linkReq);
    itsMe.linkBandWidth   = htonl(bandWidth);
    itsMe.linkEchoRequired= htonl(echoMode);
    itsMe.n               = htons(n);
    itsMe.k               = htons(k);

    debugMsg(dbg_App_Verbose,
             "heartBeat",
             "-----------------------\n"
             "sending linkMsg:: Request Connection to=[%s:%d] myId=%d\n"
             "-----------------------------------------------------------\n",
             remoteAddr->getStrValue(),
             remoteAddr->getPort(),
             localNodeIdentifier
             );

    if (internalSock->writeTo(*remoteAddr, &itsMe, sizeof(itsMe)) == -1)
    {
        NOTIFY("linkHarbinger_t::heartBeat:: could not send irouter "
               "connection request...\n"
              );
    }
}


linkHarbinger_t::~linkHarbinger_t(void)
{
    debugMsg(dbg_App_Paranoic, "~linkHarbinger", "Destroying linkHarbinger_t");
}



bool
is_ok_irouter_version(u16 iMajorVer, u16 iMinorVer)
{
    if ((iMajorVer != IROUTER_MAJOR_VERSION) ||
        (iMinorVer != IROUTER_MINOR_VERSION))
    {
        NOTIFY("is_ok_irouter_version:: Incompatible irouter versions "
               "my version=[%d.%d] and request FS version=[%d.%d]\n",
               IROUTER_MAJOR_VERSION, IROUTER_MINOR_VERSION,
               iMajorVer,
               iMinorVer
              );
        return false;
    }

    return true;
}

//---------------------------------------------------
//
// mcastLinkHarbinger_t
//    envio de peticion de conexion multicast SSM
//    envio periodico para refresco de estado blando
//
//---------------------------------------------------


mcastLinkHarbinger_t::mcastLinkHarbinger_t(transApp_t *app,
                                           const char *remote,
                                           int bw,
                                           bool echo,
                                           int new_n,
                                           int new_k
                                          )
: simpleTask_t(/*LM_TIMEOUT*/2*1000000),
  bandWidth(bw),
  echoMode(echo),
  remoteUniqueId(0),
  n(new_n),
  k(new_k),
  myApp(app)
{
    debugMsg(dbg_App_Verbose,
             "mcastLinkHarbinger_t",
             "---------------------------------\n"
             "Conectando al mcastServer [%s]\n"
             "---------------------------------\n",
             remote
            );

    int iLinkPort = app->irouterParam->getCtrlPort();
    char myPort[16];
    memset(myPort, 0, 16);
    sprintf(myPort, "%d", iLinkPort);
    internalSock = myApp->targetMgr->sockPool->getSock(NULL, iLinkPort);
    remoteAddr = new inetAddr_t(remote, myPort, serviceAddr_t::DGRAM);

    debugMsg(dbg_App_Verbose,
             "mcastLinkHarbinger_t",
             "Mcast link harbinger built"
            );
}

char *
mcastLinkHarbinger_t::getIpMsg(void)
{
    // New  mcastMsg = IP correct format
    char *almacen = new char[512];
    memset(almacen, 0, 512);
    char *tmp = myApp->irouterParam->getmyip();
    strcpy(almacen, tmp);
    strcat(almacen, "W");
    strcat(almacen, "W");

    return almacen;
}


void
mcastLinkHarbinger_t::heartBeat(void)
{
    linkMsg_t itsMe;

    itsMe.iMajorVer       = htons(IROUTER_MAJOR_VERSION);
    itsMe.iMinorVer       = htons(IROUTER_MINOR_VERSION);
    itsMe.msgType         = (msgID_e)htonl(mcastLinkReq);
    itsMe.linkBandWidth   = htonl(bandWidth);
    itsMe.linkEchoRequired= htonl(echoMode);
    itsMe.n               = htons(n);
    itsMe.k               = htons(k);

    memset(itsMe.multicastMsg,0,512);
    char *fuentes         =    getIpMsg();
    strncpy(itsMe.multicastMsg,fuentes,strlen(fuentes));
    delete[] fuentes;

    debugMsg(dbg_App_Paranoic,
             "Mcast heartBeat",
             "Sending SSM request with my own IP: [%s]",
             itsMe.multicastMsg
            );

    debugMsg(dbg_App_Verbose,
             "heartBeat",
             "-----------------------\n"
             "sending mcastLinkMsg:: Request Connection to=[%s:%d] myId=%d\n"
             "-----------------------------------------------------------\n",
             remoteAddr->getStrValue(),
             remoteAddr->getPort(),
             localNodeIdentifier
            );

    if (internalSock->writeTo(*remoteAddr, &itsMe, sizeof(itsMe)) == -1)
    {
        NOTIFY("mcastLinkHarbinger_t::heartBeat:: could not send irouter "
               "connection request...\n"
              );
    }
}


mcastLinkHarbinger_t::~mcastLinkHarbinger_t(void)
{
    debugMsg(dbg_App_Paranoic, "~mcastLinkHarbinger", "Destructor invoked");
}



bool
mcast_is_ok_irouter_version(u16 iMajorVer, u16 iMinorVer)
{
    if ((iMajorVer != IROUTER_MAJOR_VERSION) ||
        (iMinorVer != IROUTER_MINOR_VERSION))
    {
        NOTIFY("mcast_is_ok_irouter_version:: Incompatible irouter versions "
               "my version=[%d.%d] and request FS version=[%d.%d]\n",
               IROUTER_MAJOR_VERSION, IROUTER_MINOR_VERSION,
               iMajorVer,
               iMinorVer
              );
        return false;
    }

    return true;
}

