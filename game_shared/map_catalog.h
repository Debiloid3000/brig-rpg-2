#pragma once
#include <algorithm>
#include <cstddef>
#include <string>
#include <vector>

namespace BrigMapCatalog {
constexpr std::size_t MaxMapIdLength=31, MaxConfigBytes=16*1024, MaxMapListBytes=64*1024;
constexpr std::size_t MaxRegisteredMaps=512, MaxConfigLineLength=256, MaxDisplayTextLength=95, MaxSettingPathLength=127;
enum class GameMode { SinglePlayer, CoOp, Deathmatch, TeamDeathmatch };

struct MapMetadata {
 std::string name,title,campaignId,campaignTitle,category,chapterId,chapterTitle;
 int chapterOrder=0,mapOrder=0;
 bool campaignStart=false,singleplayer=true,coop=false,deathmatch=false,teamDeathmatch=false;
 GameMode defaultMode=GameMode::SinglePlayer;
 bool defaultModeExplicit=false;
 std::string forcePlayerModels,globalSoundList;
 bool hasForcePlayerModelsSetting=false,hasGlobalSoundListSetting=false,hasConfig=false;
};
struct ParseIssue { std::size_t line=0; std::string message; };

inline std::string ToLowerAscii(const std::string& s) {
 std::string r=s; for(char& c:r) if(c>='A'&&c<='Z') c=static_cast<char>(c-'A'+'a'); return r;
}
inline std::string Trim(const std::string& s) {
 const auto a=s.find_first_not_of(" \t\r\n"); if(a==std::string::npos) return {};
 return s.substr(a,s.find_last_not_of(" \t\r\n")-a+1);
}
inline bool IsIdentifier(const std::string& s,std::size_t max=MaxMapIdLength) {
 if(s.empty()||s.size()>max||s=="."||s=="..") return false;
 for(unsigned char c:s) if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='-')) return false;
 return true;
}
inline bool IsValidMapId(const std::string& s) { return IsIdentifier(s,MaxMapIdLength); }
inline bool IsSafeDisplayText(const std::string& s,std::size_t max=MaxDisplayTextLength) {
 if(s.size()>max) return false; for(unsigned char c:s) if(c<0x20||c==0x7f) return false; return true;
}
inline bool IsSafePlayerModelList(const std::string& s) {
 if(s.size()>MaxSettingPathLength) return false; if(s.empty()) return true;
 std::size_t p=0,count=0;
 while(p<s.size()) {
  while(p<s.size()&&(s[p]==','||s[p]==';'||s[p]==' '||s[p]=='\t')) ++p;
  if(p==s.size()) break;
  const auto start=p; while(p<s.size()&&s[p]!=','&&s[p]!=';'&&s[p]!=' '&&s[p]!='\t') ++p;
  if(!IsIdentifier(s.substr(start,p-start),31)||++count>32) return false;
 }
 return count>0;
}
inline bool IsSafeSoundListPath(const std::string& path)
{
	const bool legacyMapsPrefix = path.size() > 11 &&
		path.compare(0, 11, "../../maps/") == 0;
	const std::string relative = legacyMapsPrefix ? path.substr(11) : path;

	if (relative.empty() || path.size() > MaxSettingPathLength ||
		(!legacyMapsPrefix && (path[0] == '/' || path[0] == '\\')) ||
		relative.find("..") != std::string::npos ||
		relative.find(':') != std::string::npos ||
		relative.find('\\') != std::string::npos ||
		relative[0] == '/')
	{
		return false;
	}

	const std::string lower = ToLowerAscii(relative);
	const bool isTextList = lower.size() >= 4 &&
		lower.compare(lower.size() - 4, 4, ".txt") == 0;
	const bool isLegacyGsr = lower.size() >= 4 &&
		lower.compare(lower.size() - 4, 4, ".gsr") == 0;
	if (!isTextList && !isLegacyGsr)
		return false;

	for (const unsigned char c : relative)
	{
		if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
			(c >= '0' && c <= '9') || c == '_' || c == '-' ||
			c == '.' || c == '/'))
		{
			return false;
		}
	}

	return true;
}
inline MapMetadata MakeDefaultMetadata(const std::string& id) {
 MapMetadata m; m.name=ToLowerAscii(id); m.title=id; m.category="uncategorized"; return m;
}
inline bool IsModeAllowed(const MapMetadata& m,GameMode mode) {
 switch(mode) { case GameMode::SinglePlayer:return m.singleplayer; case GameMode::CoOp:return m.coop;
 case GameMode::Deathmatch:return m.deathmatch; case GameMode::TeamDeathmatch:return m.teamDeathmatch; default:return false; }
}
inline const char* GameModeName(GameMode m) {
 switch(m) { case GameMode::CoOp:return "coop"; case GameMode::Deathmatch:return "deathmatch";
 case GameMode::TeamDeathmatch:return "team_deathmatch"; default:return "singleplayer"; }
}
inline bool ParseGameMode(const std::string& s,GameMode& mode) {
 const auto v=ToLowerAscii(s);
 if(v=="singleplayer"||v=="single_player"||v=="sp") mode=GameMode::SinglePlayer;
 else if(v=="coop"||v=="co_op") mode=GameMode::CoOp;
 else if(v=="deathmatch"||v=="dm") mode=GameMode::Deathmatch;
 else if(v=="team_deathmatch"||v=="teamdeathmatch"||v=="tdm") mode=GameMode::TeamDeathmatch;
 else return false; return true;
}
inline void ChooseAvailableDefaultMode(MapMetadata& m,std::vector<ParseIssue>* issues=nullptr) {
 if(m.defaultModeExplicit&&IsModeAllowed(m,m.defaultMode)) return;
 if(m.defaultModeExplicit&&issues) issues->push_back({0,"brig_default_mode is disabled for this map; selected first enabled mode"});
 m.defaultModeExplicit=false;
 if(m.singleplayer)m.defaultMode=GameMode::SinglePlayer; else if(m.coop)m.defaultMode=GameMode::CoOp;
 else if(m.deathmatch)m.defaultMode=GameMode::Deathmatch; else if(m.teamDeathmatch)m.defaultMode=GameMode::TeamDeathmatch;
 else m.defaultMode=GameMode::SinglePlayer;
}
inline bool ParseBool01(const std::string& s,bool& v) { if(s=="0"){v=false;return true;} if(s=="1"){v=true;return true;} return false; }
inline bool ParseOrder(const std::string& s,int& v) {
 if(s.empty()||s.size()>4)return false; int n=0; for(unsigned char c:s){if(c<'0'||c>'9')return false;n=n*10+(c-'0');}
 if(n>9999)return false; v=n; return true;
}
inline bool IsTrailingComment(const std::string& s) {
 const auto r=Trim(s); return r.empty()||r[0]=='#'||(r.size()>=2&&r[0]=='/'&&r[1]=='/');
}

// Strict typed parser. It never executes commands or writes arbitrary CVARs.
inline bool ParseMapConfig(const std::string& text,MapMetadata& m,std::vector<ParseIssue>* issues=nullptr) {
 auto report=[&](std::size_t n,const std::string& s){if(issues)issues->push_back({n,s});};
 if(!IsValidMapId(m.name)||text.size()>MaxConfigBytes){report(0,"invalid map id or CFG exceeds 16 KiB");return false;}
 std::vector<std::string> seen; std::size_t p=0,lineNo=0;
 while(p<=text.size()) {
  const auto e=text.find('\n',p),end=e==std::string::npos?text.size():e;
  std::string line=text.substr(p,end-p); ++lineNo; p=e==std::string::npos?text.size()+1:e+1;
  if(!line.empty()&&line.back()=='\r')line.pop_back();
  if(line.size()>MaxConfigLineLength){report(lineNo,"line exceeds 256 bytes");continue;}
  line=Trim(line); if(line.empty()||line[0]=='#'||(line.size()>1&&line[0]=='/'&&line[1]=='/'))continue;
  const auto ke=line.find_first_of(" \t"); if(ke==std::string::npos){report(lineNo,"expected key and value");continue;}
  const std::string key=ToLowerAscii(line.substr(0,ke)); const auto vs=line.find_first_not_of(" \t",ke);
  if(vs==std::string::npos){report(lineNo,"missing value");continue;}
  std::string value;
  if(line[vs]=='"') { const auto qe=line.find('"',vs+1); if(qe==std::string::npos||!IsTrailingComment(line.substr(qe+1))){report(lineNo,"malformed quoted value or trailing data");continue;} value=line.substr(vs+1,qe-vs-1); }
  else { const auto ve=line.find_first_of(" \t",vs); value=ve==std::string::npos?line.substr(vs):line.substr(vs,ve-vs); if(ve!=std::string::npos&&!IsTrailingComment(line.substr(ve))){report(lineNo,"unexpected data after value");continue;} }
  if(value.size()>MaxSettingPathLength||!IsSafeDisplayText(value,MaxSettingPathLength)){report(lineNo,"value too long or contains control characters");continue;}
  const bool known=key=="brig_title"||key=="brig_campaign"||key=="brig_campaign_title"||key=="brig_category"||
   key=="brig_chapter"||key=="brig_chapter_title"||key=="brig_chapter_order"||key=="brig_map_order"||
   key=="brig_campaign_start"||key=="brig_singleplayer"||key=="brig_coop"||key=="brig_deathmatch"||
   key=="brig_team_deathmatch"||key=="brig_default_mode"||key=="forcepmodels"||key=="globalsoundlist";
  if(!known){report(lineNo,"unsupported map CFG key; commands and arbitrary CVARs are not allowed");continue;}
  if(std::find(seen.begin(),seen.end(),key)!=seen.end()){report(lineNo,"duplicate key ignored");continue;} seen.push_back(key);
  bool valid=true;
  if(key=="brig_title"){valid=IsSafeDisplayText(value);if(valid)m.title=value;}
  else if(key=="brig_campaign"){valid=value.empty()||IsIdentifier(value);if(valid)m.campaignId=ToLowerAscii(value);}
  else if(key=="brig_campaign_title"){valid=IsSafeDisplayText(value);if(valid)m.campaignTitle=value;}
  else if(key=="brig_category"){valid=value.empty()||IsIdentifier(value);if(valid)m.category=ToLowerAscii(value);}
  else if(key=="brig_chapter"){valid=value.empty()||IsIdentifier(value);if(valid)m.chapterId=ToLowerAscii(value);}
  else if(key=="brig_chapter_title"){valid=IsSafeDisplayText(value);if(valid)m.chapterTitle=value;}
  else if(key=="brig_chapter_order")valid=ParseOrder(value,m.chapterOrder);
  else if(key=="brig_map_order")valid=ParseOrder(value,m.mapOrder);
  else if(key=="brig_campaign_start")valid=ParseBool01(value,m.campaignStart);
  else if(key=="brig_singleplayer")valid=ParseBool01(value,m.singleplayer);
  else if(key=="brig_coop")valid=ParseBool01(value,m.coop);
  else if(key=="brig_deathmatch")valid=ParseBool01(value,m.deathmatch);
  else if(key=="brig_team_deathmatch")valid=ParseBool01(value,m.teamDeathmatch);
  else if(key=="brig_default_mode"){valid=ParseGameMode(value,m.defaultMode);if(valid)m.defaultModeExplicit=true;}
  else if(key=="forcepmodels"){valid=IsSafePlayerModelList(value);if(valid){m.forcePlayerModels=value;m.hasForcePlayerModelsSetting=true;}}
  else if(key=="globalsoundlist"){valid=value.empty()||IsSafeSoundListPath(value);if(valid){m.globalSoundList=value;m.hasGlobalSoundListSetting=true;}}
  if(!valid)report(lineNo,"invalid value for "+key);
 }
 ChooseAvailableDefaultMode(m,issues); m.hasConfig=true; return true;
}

inline bool ParseMapList(const std::string& text,std::vector<std::string>& ids,std::vector<ParseIssue>* issues=nullptr) {
 ids.clear(); if(text.size()>MaxMapListBytes){if(issues)issues->push_back({0,"map list exceeds 64 KiB"});return false;}
 std::size_t p=0,lineNo=0;
 while(p<=text.size()) {
  const auto e=text.find('\n',p),end=e==std::string::npos?text.size():e;
  std::string line=Trim(text.substr(p,end-p)); ++lineNo; p=e==std::string::npos?text.size()+1:e+1;
  if(line.empty()||line[0]=='#'||(line.size()>1&&line[0]=='/'&&line[1]=='/'))continue;
  const auto te=line.find_first_of(" \t"); const std::string token=line.substr(0,te);
  if(te!=std::string::npos&&!IsTrailingComment(line.substr(te))){if(issues)issues->push_back({lineNo,"extra data after map identifier"});continue;}
  if(!IsValidMapId(token)){if(issues)issues->push_back({lineNo,"invalid map identifier; paths and extensions are not allowed"});continue;}
  const auto normalized=ToLowerAscii(token);
  if(std::find(ids.begin(),ids.end(),normalized)!=ids.end()){if(issues)issues->push_back({lineNo,"duplicate map identifier ignored"});continue;}
  if(ids.size()>=MaxRegisteredMaps){if(issues)issues->push_back({lineNo,"512 map limit reached"});break;}
  ids.push_back(normalized);
 }
 return true;
}
inline bool IsCatalogMapUsable(const std::string& id,bool bspExists) { return IsValidMapId(id)&&bspExists; }
inline bool IsGoldSrcBspHeader(const unsigned char* h,std::size_t n) {
 return h&&n>=4&&h[0]==30&&h[1]==0&&h[2]==0&&h[3]==0;
}
} // namespace BrigMapCatalog
