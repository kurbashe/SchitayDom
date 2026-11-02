// Standalone wire protocol. Keep IDs unique across the installed mod set.
enum GMapKind
{
    SERVER = 0,
    ZONE = 1,
    RESTRICTED = 2,
    PERSONAL = 3,
    GROUP = 4
}

class GMapProtocol
{
    static const int VERSION = 1;
    static const int REQUEST = 7942311;
    static const int RESPONSE = 7942312;
}

class GMapMarker
{
    string Id;
    string Name;
    vector Position;
    int Color = -16744449;
    string Icon = "pin";
    int Kind;
    float Radius;
    string OwnerUid;
}

class GMapPeer
{
    string Uid;
    string Name;
}

class GMapSnapshot
{
    int Version = 1;
    ref array<ref GMapMarker> Markers = new array<ref GMapMarker>;
    ref array<ref GMapPeer> OnlinePlayers = new array<ref GMapPeer>;
    ref array<ref GMapPeer> GroupMembers = new array<ref GMapPeer>;
    string MyUid;
    string GroupId;
    string GroupName;
    string InviteFrom;
    string Message;
    string WorldName;
    bool IsLeader;
    bool Ready;
    bool ShowPlayerPosition = true;
    int MaxPersonalMarkers = 80;
    int MaxGroupMarkers = 100;
}

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

// Client API: UI watches Revision and reads State after a valid server response.
class GMapClient
{
    static ref GMapSnapshot State = new GMapSnapshot;
    static int Revision;

    static void Reset()
    {
        State = new GMapSnapshot;
        Revision = 0;
    }

    static void Send(string command, string id = "", string name = "", vector position = "0 0 0", int color = -16744449, string icon = "pin", bool shared = false)
    {
        if (!GetGame() || !GetGame().IsClient()) return;
        ScriptRPC rpc = new ScriptRPC;
        rpc.Write(GMapProtocol.VERSION);
        rpc.Write(command);
        rpc.Write(id);
        rpc.Write(name);
        rpc.Write(position);
        rpc.Write(color);
        rpc.Write(icon);
        rpc.Write(shared);
        rpc.Send(null, GMapProtocol.REQUEST, true, null);
    }

    static void Receive(ParamsReadContext ctx)
    {
        int version;
        string json;
        if (!ctx.Read(version) || version != GMapProtocol.VERSION) return;
        if (!ctx.Read(json) || json.Length() > 500000) return;
        GMapSnapshot snapshot;
        string error;
        if (!JsonFileLoader<GMapSnapshot>.LoadData(json, snapshot, error) || !snapshot) return;
        if (snapshot.Version != 1 || !snapshot.Markers || !snapshot.OnlinePlayers || !snapshot.GroupMembers) return;
        State = snapshot;
        Revision++;
    }
}
