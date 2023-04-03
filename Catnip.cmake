
catnip_package(dvm DEFAULT all)

catnip_add_preset(ds_calico
	TOOLSET    NDS
	BUILD_TYPE Release
	CACHE
		DKP_NDS_PLATFORM_LIBRARY=calico
)

catnip_add_preset(ds_libnds
	TOOLSET    NDS
	BUILD_TYPE Release
	CACHE
		DKP_NDS_PLATFORM_LIBRARY=libnds
)

catnip_add_preset(cube
	TOOLSET    GameCube
	BUILD_TYPE Release
)

catnip_add_preset(wii
	TOOLSET    Wii
	BUILD_TYPE Release
)
