/*
===========================================================================
Holstered guns (singleplayer), see cg_holster.cpp
===========================================================================
*/

#ifndef CG_HOLSTER_H
#define CG_HOLSTER_H

// Draws the guns this character carries but is not holding on his body (after his model is added,
// with the angles and origin his model was drawn with)
void CG_HolsteredWeapons(centity_t* cent, const vec3_t g2Angles, const vec3_t origin, int playerRenderfx,
	float shadowPlane);

// Whether a gun has the left or right hip place of this character (the holstered saber then goes to the
// front of that hip, wp_saber.cpp)
qboolean CG_HolsterHipTaken(int entNum, qboolean left);

// Frees the holstered gun models and forgets the loaded holster.cfg files (CG_Shutdown)
void CG_HolsterShutdown();

#endif // CG_HOLSTER_H
