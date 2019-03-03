//--------------------------------------------------------------------------------------
// By XU, Tianchen
//--------------------------------------------------------------------------------------

#ifndef U_MUTEX
#define	U_MUTEX			u4
#endif

#ifndef RWTexture
#define	RWTexture		RWTexture3D
#endif

#define	mutexLock(x)	{	\
							uint lock;				\
							[allow_uav_condition]	\
							for (uint i = 0; i < 0xffffffff; ++i)	\
							{	\
								InterlockedExchange(g_rwMutex[x], 1, lock);	\
								DeviceMemoryBarrier();						\
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
