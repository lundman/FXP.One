// $Id: engine.h,v 1.8 2012/03/16 06:57:58 lundman Exp $
//
// Jorgen Lundman 10th October 2003.

#ifndef ENGINE_H_INCLUDED
#define ENGINE_H_INCLUDED

// Defines.

#define VERSION_MAJOR 2
#define VERSION_MINOR 0

// These should go in header for protocol
#define PROTOCOL_MAJOR 1
#define PROTOCOL_MINOR 3






// Variables

extern int engine_running;
extern int engine_nodelay;
extern int engine_firstrun;






// Functions

// int    main            ( int, char ** );
void      arguments       ( int, char ** );
void      engine_daemonise( void );
void      engine_workdir  ( void );



#endif
