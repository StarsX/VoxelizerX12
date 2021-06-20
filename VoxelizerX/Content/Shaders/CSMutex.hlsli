//--------------------------------------------------------------------------------------
// Copyright (c) XU, Tianchen. All rights reserved.
//--------------------------------------------------------------------------------------

#ifndef U_MUTEX
#define	U_MUTEX			u3
#endif

#ifndef RWTexture
#define	RWTexture		RWTexture3D
#endif

#define	mutexLock(x)	{	\
							uint lock = 1;			\
							[allow_uav_condition]	\
							for (uint i = 0; i < 0xffffffff; ++i)	\
							{	\
								InterlockedCompareExchange(g_rwMutex[x], 0, 1, lock);	\
								DeviceMemoryBarrier();	\
								if (lock != 1)	\
								{
#define	mutexUnlock(x)				g_rwMutex[x] = 0;	\
									break;				\
								}	\
							}	\
						}

//--------------------------------------------------------------------------------------
// Unordered access texture
//--------------------------------------------------------------------------------------
RWTexture<uint>	g_rwMutex	: register (U_MUTEX);
