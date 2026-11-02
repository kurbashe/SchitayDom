// Client-only presentation preferences. No player/group authority is stored here.
class GMapPreferences
{
    int Version = 1;
    bool ShowPlayer = true;
    bool ShowNames = true;
    bool ShowZones = true;
    float Scale = 0.22;
    vector Center = "7500 0 7500";
    ref array<string> Hidden = new array<string>;

    static GMapPreferences Load()
    {
        GMapPreferences value = new GMapPreferences;
        string error;
        if (FileExist("$profile:GorillaMapClient.json"))
        {
            GMapPreferences loaded;
            if (JsonFileLoader<GMapPreferences>.LoadFile("$profile:GorillaMapClient.json", loaded, error) && loaded && loaded.Version == 1)
                value = loaded;
        }
        if (!value.Hidden)
            value.Hidden = new array<string>;
        value.Scale = Math.Clamp(value.Scale, 0.01, 1.0);
        // Limit local state growth if server marker ids have changed over time.
        if (value.Hidden.Count() > 1000)
            value.Hidden.Clear();
        return value;
    }

    void Save()
    {
        string error;
        if (!JsonFileLoader<GMapPreferences>.SaveFile("$profile:GorillaMapClient.json", this, error))
            Print("[GORILLA MAP] Could not save local presentation settings: " + error);
    }
};

class GMapRow
{
    Widget Root;
    ButtonWidget Select;
    ButtonWidget Visibility;
    ButtonWidget Edit;
    ButtonWidget Delete;
    ref GMapMarker Marker;
};
