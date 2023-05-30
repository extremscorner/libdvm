
catnip_package(dvm DEFAULT all)

catnip_add_preset(ds
	TOOLSET    NDS
	BUILD_TYPE Release
)

catnip_add_preset(cube
	TOOLSET    GameCube
	BUILD_TYPE Release
)

catnip_add_preset(wii
	TOOLSET    Wii
	BUILD_TYPE Release
)
