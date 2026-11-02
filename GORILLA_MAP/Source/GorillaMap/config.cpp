class CfgPatches
{
    class GorillaMap
    {
        units[] = {};
        weapons[] = {};
        requiredVersion = 0.1;
        requiredAddons[] = {"DZ_Data", "DZ_Scripts"};
    };
};

class CfgMods
{
    class GorillaMap
    {
        dir = "GorillaMap";
        name = "GORILLA MAP";
        author = "GORILLA";
        version = "0.1.0";
        type = "mod";
        inputs = "GorillaMap/GUI/inputs.xml";
        dependencies[] = {"Game", "World", "Mission"};
        class defs
        {
            class gameScriptModule
            {
                value = "";
                files[] = {"GorillaMap/Scripts/3_Game"};
            };
            class worldScriptModule
            {
                value = "";
                files[] = {"GorillaMap/Scripts/4_World"};
            };
            class missionScriptModule
            {
                value = "";
                files[] = {"GorillaMap/Scripts/5_Mission"};
            };
        };
    };
};
