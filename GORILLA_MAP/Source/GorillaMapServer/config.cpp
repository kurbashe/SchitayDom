class CfgPatches
{
    class GorillaMapServer
    {
        units[] = {};
        weapons[] = {};
        requiredVersion = 0.1;
        requiredAddons[] = {"DZ_Data", "DZ_Scripts", "GorillaMap"};
    };
};

class CfgMods
{
    class GorillaMapServer
    {
        dir = "GorillaMapServer";
        name = "GORILLA MAP Server";
        author = "GORILLA";
        version = "0.1.2";
        type = "mod";
        dependencies[] = {"Game", "World", "Mission"};
        class defs
        {
            class gameScriptModule
            {
                value = "";
                files[] = {"GorillaMapServer/Scripts/3_Game"};
            };
            class worldScriptModule
            {
                value = "";
                files[] = {"GorillaMapServer/Scripts/4_World"};
            };
            class missionScriptModule
            {
                value = "";
                files[] = {"GorillaMapServer/Scripts/5_Mission"};
            };
        };
    };
};
