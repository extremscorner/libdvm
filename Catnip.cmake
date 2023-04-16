
catnip_package(dvm DEFAULT ds cube wii)

catnip_add_preset(ds
	TOOLSET    NDS
	BUILD_TYPE Release
)

catnip_add_preset(ds_old
	TOOLSET    NDS
	BUILD_TYPE Release
	CACHE
		DKP_NDS_OLD_ROOT=TRUE
)

catnip_add_preset(cube
	TOOLSET    GameCube
	BUILD_TYPE Release
)

catnip_add_preset(wii
	TOOLSET    Wii
	BUILD_TYPE Release
)
