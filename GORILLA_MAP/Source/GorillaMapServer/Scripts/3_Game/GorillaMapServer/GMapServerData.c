// Server-only configuration and persistent records. Shared DTOs come from GorillaMap.
class GMapConfig
{
    int Version = 1;
    string WorldName = "chernarusplus";
    float WorldSize = 15360;
    bool ShowPlayerPosition = true;
    int MaxPersonalMarkers = 80;
    int MaxGroupMarkers = 100;
    int MaxGroupMembers = 8;
    int MaxGroups = 1000;
    int MaxAccounts = 10000;
    ref array<ref GMapMarker> Markers = new array<ref GMapMarker>;
}

class GMapAccount
{
    string Uid;
    string Name;
    ref array<ref GMapMarker> Markers = new array<ref GMapMarker>;
}

class GMapGroup
{
    string Id;
    string Name;
    string LeaderUid;
    ref array<string> Members = new array<string>;
    ref array<ref GMapMarker> Markers = new array<ref GMapMarker>;
}

class GMapStore
{
    int Version = 1;
    int NextId = 1;
    ref array<ref GMapAccount> Accounts = new array<ref GMapAccount>;
    ref array<ref GMapGroup> Groups = new array<ref GMapGroup>;
}

