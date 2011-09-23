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

#include "CtrlInterface.h"

static int DEBUG = 0;

static char *HELP_FUNCTION = "help";
static char *EXIT_FUNCTION = "exit";

char **buildArgs(char *cadena, int *argc);
int deleteArgs(char **args);

ControlInterface *ControlInterfaceNew(int port) {
   ControlInterface *ci = NULL;
   
   if ( !(ci = (ControlInterface *)malloc(sizeof(ControlInterface))) ) {
     perror("ControlInterfaceNew::malloc");
      return NULL;
   }
   ci -> port = port;
   ci -> socket = -1;
   ci -> nDirectives = 0;
   ci -> started = 0;
   ci -> listen = 0;
   return ci;
}

int ControlInterfaceDelete(ControlInterface *ci) {
   int i;

   if (ControlInterfaceStop(ci) < 0 ) {
      return -1;
   }
  
   if ( close(ci -> socket) < 0 ) return -1;
   for ( i = 0 ; i < ci -> nDirectives ; i++ ) {
      free(ci -> directives[i].directive);
      free(ci -> directives[i].help);
   }
   free(ci);
   return 1;
}

// Devuelve el puerto en el que escucha o -1 si hay error
int ControlInterfaceInit(ControlInterface *ci) {
#ifdef IPV6
  Address *local_addr;
#else
  sockaddr_in *local_addr;
#endif
  struct linger opt;
  
#ifdef IPV6
  local_addr = my_getaddrinfo(INET_ANY, ci -> port, "ControlInterfaceInit");
  if ( (ci -> socket = my_socket(local_addr, SOCK_STREAM)) < 0) {
    //    perror("ControlInterfaceInit::socket");
    return -1;
  }
#else
  struct sockaddr_in sinaddr;
  socklen_t sockaddrLen;
  if ( (ci -> socket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    //    perror("DataProtocolInit::socket");
    return -1;
  }
#endif
  opt.l_onoff = 1;
  opt.l_linger = 0;
  setsockopt(ci->socket, SOL_SOCKET, SO_LINGER, (char*)&opt, sizeof(opt));
  
#ifdef IPV6
  if ( my_bind(ci -> socket, local_addr) < 0 ) {
    perror("ControlInterfaceInit::bind");
    return -1;
  }
  ci -> port = getport(local_addr);
  freeaddrinfo(local_addr);
#else
  bzero((void *)&sinaddr, sizeof(sinaddr));
  sinaddr.sin_family= AF_INET;
  sinaddr.sin_port= htons(ci -> port);
  sinaddr.sin_addr.s_addr= INADDR_ANY;
  if(bind(ci -> socket, (sockaddr *)&sinaddr, sizeof(sinaddr))< 0) {
    return -1;
  }
  sockaddrLen = sizeof(sinaddr);
  if (getsockname(ci -> socket, (struct sockaddr *)&sinaddr, &sockaddrLen) < 0) {
    perror("ControlInterfaceInit::getsockname");
    return -1;
  }
  ci -> port = ntohs(sinaddr.sin_port);
#endif  
  
  return ci -> port;
}

#define MAX_BUFFER 64*1024

char *functionHelp(ControlInterface *ci, int nargs, char *args[]) {
  int i;
  char result[MAX_BUFFER] = "";
  // Solo hay un help()
  if (nargs == 1) {
    for ( i = 0 ; i < ci -> nDirectives ; i++ ) {      
      strcat(result, ci -> directives[i].directive);
      strcat(result, " ");
      strcat(result, ci -> directives[i].help);
      strcat(result, "\n");
    }
    strcat(result, "OK\n");
    return strdup(result);
  }
  // Hay un help(function)
  if (nargs == 2) {
    for( i = 0 ; i < ci -> nDirectives ; i++ ) {
      if (!strcmp(ci -> directives[i].directive, args[1]) ) {
	strcat(result, ci -> directives[i].directive);
	strcat(result, " ");
	strcat(result, ci -> directives[i].help);
	strcat(result, "\nOK\n");
	return strdup(result);
      }
    }
    return strdup("Function not found\n");
  }
  return strdup("ERROR\n");
}

int ControlInterfaceRegisterDirective(ControlInterface *ci, const char *directive, Function function, const char *help) {
  
  // Aunque podria parecer que hace falta un candado si se hace un register dinamico mientras que esta funcionando 
  // el thread, en realidad no hace falta ya que la nueva funcion es la ultima y hasta que no esta insertada no se
  // incrementa el valor de nDirectives
  ci -> directives[ci -> nDirectives].function = function;
  ci -> directives[ci -> nDirectives].directive = strdup(directive);
  if (!ci -> directives[ci -> nDirectives].directive) {
    //    perror("ControlInterfaceRegisterDirective::strdup(1)");
    return -1;
  }
  ci -> directives[ci -> nDirectives].help = strdup(help);
  if (!ci -> directives[ci -> nDirectives].help) {
    perror("ControlInterfaceRegisterDirective::strdup(2)");
    //    return -1;
  }
  ci -> nDirectives++;
  return 1;
}

int ControlInterfaceRegisterDirectiveSet(ControlInterface *ci, int nDirectives, Directive *d) {
  int i;
  
  for ( i = 0 ; i < nDirectives ; i++ ) {
    if (ControlInterfaceRegisterDirective(ci, d[i].directive, d[i].function, d[i].help) < 0 ) {
      return -1;
    }
  }
  return 1;
}

void *ControlInterfaceListener(void *params) {
  
  fd_set rdFd; 
  struct timeval tv;
  int fileDesc;
  
  
  ControlInterface *ci;  

  ci = (ControlInterface *)params;   
  
  while (ci -> listen) {
    int newSocket;
    int connected;

#ifdef IPV6  
    Address remote_addr;

    if ( (newSocket = my_accept(ci -> socket, &remote_addr)) < 0 ) {
      perror("ControlInterfaceListener::accept");
      return NULL;
    }
#else
    struct sockaddr_in	sin;
    socklen_t sockaddrLen;

    sockaddrLen = sizeof(sin);
    if ( (newSocket = accept(ci -> socket, (struct sockaddr *)&sin, &sockaddrLen)) < 0 ) {
      // perror("ControlInterfaceListener::accept");
      return NULL;
    }
#endif
    connected = 1;
    while(connected) {
      
      FD_ZERO(&rdFd);
      FD_SET(newSocket, &rdFd);
      tv.tv_sec= 0;
      tv.tv_usec= 1000000;
      fileDesc = select(newSocket+1, &rdFd, NULL, NULL, &tv);
      if ( fileDesc > 0) {
	if(FD_ISSET(newSocket, &rdFd)) {
	  if (  ControlInterfaceDeal(ci, newSocket) < 0 ) {
	    connected = 0;
	  }
	}
      }
      pthread_testcancel();
    }
  }
  return NULL;
}

int ControlInterfaceListen(ControlInterface *ci) {

#ifdef IPV6
  if ( my_listen(ci -> socket, 1) < 0 ) {
    perror("ControlInterfaceStart::listen");
    return -1;
  }
#else
  if ( listen(ci -> socket, 1) < 0 ) {
    perror("ControlInterfaceStart::listen");
    return -1;
  }
#endif
  return 1;
}

int ControlInterfaceStart(ControlInterface *ci) {
  // Ya esta funcionando
  if ( ci -> started )
    return 1;
  
  /*
    pthread_attr_t attr;
    struct sched_param sched_param;
    
    if (pthread_attr_init(&attr) != 0) {
    perror("AudioDeviceStart::pthread_attr_init");
    return -1;
    }
    if (pthread_attr_setschedpolicy(&attr, SCHED_RR) != 0) {
    perror("AudioDeviceStart::pthread_attr_setschedpolicy");
    return -1;
    }
    if (sched_getparam(0, &sched_param) < 0) {
    perror("AudioDeviceStart::sched_getparam");
    return -1;
    }
    sched_param.sched_priority = sched_get_priority_max(SCHED_RR);  
    if (pthread_attr_setschedparam(&attr, &sched_param) != 0) {
    perror("AudioDeviceStart::pthread_attr_setschedparam");
    return -1;
    }
  */
  ci -> listen = 1;
  
  if ( pthread_create(&(ci -> netThread), NULL, ControlInterfaceListener, ci) != 0 ) {
    perror("ControlInterfaceStart::pthread_create");
    return -1;
  }
  
  ci -> started = 1;
  return 1;
}

int ControlInterfaceStop(ControlInterface *ci) {
  int val[1];
  
  if ( !ci -> started )
    return 1;
  ci -> listen = 0;
  usleep(100000);
  pthread_cancel(ci -> netThread);//, (void **)&val);
  ci -> started = 0; 
  return 1;
}

int ControlInterfaceDeal(ControlInterface *ci, int newSocket) {
  const char mensaje_error[] = "Unknow command or Sintax Error\n";
  char buffer[MAX_BUFFER];
  char *result;
  int readed;
  
  char **args;
  int nparams;
  int i;

  memset(buffer, 0, MAX_BUFFER);
  result = NULL;
  readed = read(newSocket, buffer, MAX_BUFFER);
  if ( readed <= 0) {
    // perror("ControlInterfaceListener::read");
    return -1;
  }
  buffer[readed] = '\0';
  if (DEBUG) printf("comando = <%s>\n", buffer);
  args = buildArgs(buffer, &nparams);
  if (args) {
    int encontrado = 0;
    // Buscan ayuda?
    if (!strcmp(args[0], HELP_FUNCTION)) {
      result = functionHelp(ci, nparams+1, args);
      write(newSocket, result, strlen(result));
      free(result);
      deleteArgs(args);
      return 1;
    }
    // Quieren salir?
    if (!strcmp(args[0], "exit")) {
      result = strdup("bye\n\n");
      write(newSocket, result, strlen(result));
      close(newSocket);
      //      connected = 0;
      free(result);
      deleteArgs(args);
      return -1;
    }
    // Es un comando ?
    for( i = 0 ; i < ci -> nDirectives ; i++ ) {
      if (!strcmp(ci -> directives[i].directive, args[0]) ) {
	result = (ci -> directives[i]).function(nparams+1, args);
	if (result) {
	  write(newSocket, result, strlen(result));
	  if (DEBUG) printf("Devuelto = <%s>\n", result);
	  free(result);
	  deleteArgs(args);
	  encontrado = 1;
	}
      }
    }
    if (!encontrado) {
      // Error
      result = strdup(mensaje_error);
      write(newSocket , result, strlen(result));
      if (DEBUG) printf("Devuelto = <%s>\n", result);
      free(result);
      deleteArgs(args);
    }
  }
  return 1;
}

#define MAX_ARGS 256
// Devuelve un array de cadenas y en argc el numero de argumentos dentro de los parentesis
// Los parametros se pueden omitir 
// Permite algunos errores como por ejemplo:
// 	comando) -> Devuelve el comando e informa de que no hay parametros
// 	comando(hola, ) -> Devuelve el comando y el primer parametro. Informa de que hay dos parametros y para el segundo devuelve una cadena vacia
//		comando as, sld)(ksdf, lsd) -> Ignora la primera lista
//		comando as, sld (ksdf, lsd) -> Ignora la primera lista
char **buildArgs(char *cadena, int *argc) {
  int i;
  char *p, *first, *last;
  size_t len;
  char **args;
  int nargs;
  char **result;
  
  args = (char **)malloc(sizeof(char *)*MAX_ARGS);
  //  printf("reservados %dbytes para args\n", sizeof(char*)*MAX_ARGS);
  if (!args) {
    //    perror("buildArgs::malloc");
    return NULL;
  }
  for ( i = 0 ; i < MAX_ARGS ; i++ ) args[i] = NULL;
  p = cadena;
  // Busca la primera letra del comando
  while (( (*p == ' ') || (*p == '\t') ) && (*p != '\0') && (*p != '\r') && (*p != '\n') ) p++;
  if ( *p == '\0' ) {
    *argc = 0;
    return NULL;
  }
  first = p;
  // Busca el final del comando
  while ( (*p != ' ') && (*p != '\t') && (*p != '(') && (*p != ')') && (*p != '\0') && (*p != '\r') && (*p != '\n')) p++;
  len = p - first + 1;
  nargs = 0;
  args[nargs] = (char *)malloc(len);
  //  printf("reservados %dbytes mas\n", len);
  strncpy(args[0], first, len-1);
  args[nargs][len-1] = '\0';
  // Busco principio de parametros
  p = index(cadena, '(');
  if (p == cadena) {
    //    perror("no hay comando, encontrado '(' al principio\n");
    *argc = 0;
    return NULL;
  }
  if ( (!p) || (*p == '\0')  || (*p == '\r') || (*p == '\n')) {
    *argc = nargs;
    return args;
  }
  // Comprobar que al menos hay un parentesis cerrando ya que hay uno abriendo
  /*
    if (!index(p, ')')) {
    //      fprintf(stderr, "La lista de parametros no esta terminada con ')'\n");
    *argc = 0;
    return NULL;
    }
  */
  // Salta el parentesis
  p++;
  // Busca la primera letra del primer parametro
  while (( (*p == ' ') || (*p == '\t') ) && (*p != '\0') && (*p != '\r') && (*p != '\n') && (*p != ')') ) p++;
  if ( (*p == '\0') || (*p == '\r') || (*p == '\n') ) {
    *argc = nargs;
    return args;
  }
  first = p;
  // Busca el final del primer parametro
  while ( (*p != ' ') && (*p != '\t') && (*p != ',') && (*p != '(') && (*p != ')') && (*p != '\0') && (*p != '\r') && (*p != '\n')) p++;
  len = p - first + 1;
  if (len > 1) {
    nargs++;
    args[nargs] = (char *)malloc(len);
    //    printf("reservados %dbytes mas para un parametro\n", len);
    strncpy(args[nargs], first, len-1);
    args[nargs][len-1] = '\0';
  }
  
  for ( ; ; ) {
    // Busca si hay mas parametros
    while ( (*p != ',') && (*p != ')') && (*p != '\0') && (*p != '\r') && (*p != '\n') ) p++;
    if (*p == ')') {
      *argc = nargs;
      return args;
    }
    // Salta la coma
    p++;
    // Busca la primera letra del primer parametro
    while (( (*p == ' ') || (*p == '\t') ) && (*p != '\0') && (*p != '\r') && (*p != '\n') && (*p != ')') ) p++;
    if ( (*p == '\0') || (*p == '\r') || (*p == '\n')) {
      *argc = nargs;
      return args;
    }
    first = p;
    // Busca el final del parametro
    while ( (*p != ' ') && (*p != '\t') && (*p != ',') && (*p != '(') && (*p != ')') && (*p != '\0') && (*p != '\r') && (*p != '\n') ) p++;
    len = p - first + 1;
    nargs++;
    args[nargs] = (char *)malloc(len);
    //    printf("reservados %dbytes mas para un parametro que no es el primero\n", len);
    strncpy(args[nargs], first, len-1);
    args[nargs][len-1] = '\0';
  }
  // Por si acaso, pero no hace falta
  *argc = nargs;
  return args;
}

// Libera la memoria reservada
int deleteArgs(char **args) {
  int i;
  
  for ( i = 0 ; i < MAX_ARGS ; i++ ) {
    if (args[i]) free (args[i]);
  }
  free (args);
  //  printf("argumentos liberados\n");
  return 1;
}







