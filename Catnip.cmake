
catnip_package(dvm DEFAULT all)

catnip_add_preset(gba
	TOOLSET    GBA
	BUILD_TYPE MinSizeRel
)

catnip_add_preset(nds
	TOOLSET    NDS
	BUILD_TYPE Release
)

catnip_add_preset(gamecube
	TOOLSET    GameCube
	BUILD_TYPE Release
)

catnip_add_preset(wii
	TOOLSET    Wii
	BUILD_TYPE Release
)
