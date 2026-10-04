#include "Runtime.hpp"
#include "ScriptEvent.hpp"
#include "MenuWidgets.hpp"
#include <Unreal/UEnum.hpp>
#include <map>
#include <Unreal/Core/Windows/AllowWindowsPlatformTypes.hpp>
#include <Windows.h>
#include <Xinput.h>
#include <deque>
#include <optional>

namespace Wardrobe {
namespace {
constexpr auto tagName=L"UI.Menu.HUB.WardrobeTransmog";
Ref page,tree,root,grid,title,footer,navbar,tabButton,previewImage;
Ref countLabel,pageLabel,setLabel,modal,modalGrid,modalTitle,modalHelp,fontStyle,gridFrame,activeFrame,categoryFrame;
std::array<Ref,5> categoryIcons,summaryNames,summaryImages;
std::map<UObject*,std::pair<Ref,Ref>> itemIcons;
std::map<std::wstring,Ref> rarityTextures;
uint64_t shownRevision=~uint64_t{};unsigned shownFocus=~0u;bool controllerInput{},paintHints=true;
constexpr auto frames=L"/Game/_Dawnwalker/UI/_Unified/SharedTextures/General/Frames/";
Ref showFunction,hubGraph;
ScriptEvent activated,deactivated,rebuilt;
Work injection;bool wantsOpen{},redraw{},built{};unsigned tileCursor{},popup{},focus{};
struct Button {Ref object,label,highlight;std::function<void()> click;int category=-1;};std::vector<Button> buttons;
std::vector<Button> controls;
struct Control {Ref parent;std::wstring caption;std::function<void()> click;Layout::Rect rect;int category=-1;};std::deque<Control> pendingControls;
std::vector<Look> filtered;std::vector<Choice> pageChoices;
std::array<bool,256> previousKeys{};WORD previousPad{};BYTE previousLeft{},previousRight{};POINT previousMouse{};uint64_t xPressed{};
std::array<bool,256> keyEdges{};
bool pressed(int key){return keyEdges[key];}
void pollKeys(){for(int key:std::initializer_list<int>{VK_ESCAPE,'A','D',VK_PRIOR,VK_NEXT,VK_TAB,'Y','F','T','S','R',VK_LEFT,VK_RIGHT,VK_UP,VK_DOWN,VK_LBUTTON,VK_RETURN}){bool down=(GetAsyncKeyState(key)&0x8000)!=0;keyEdges[key]=down&&!previousKeys[key];previousKeys[key]=down;if(keyEdges[key]&&controllerInput){controllerInput=false;paintHints=true;}}}
void tag(FProperty* p,void* base,const wchar_t* text){
    if(!p||!p->IsA<FStructProperty>())throw std::runtime_error("Expected gameplay tag");auto st=static_cast<FStructProperty*>(p)->GetStruct();auto field=property(st,L"TagName");assignText(field,p->ContainerPtrToValuePtr<void>(base),text);
}
bool ourTag(UFunction* fn,void* params){
    for(auto p:fn->ForEachProperty())if(p->IsA<FStructProperty>()&&(p->GetPropertyFlags()&CPF_Parm)){
        auto st=static_cast<FStructProperty*>(p)->GetStruct();auto n=property(st,L"TagName");if(n)return text(n,p->ContainerPtrToValuePtr<void>(params))==tagName;
    }return false;
}
UObject* widget(const wchar_t* cls){auto p=construct(cls,tree.get());if(!p)throw std::runtime_error("Widget class unavailable");return p;}
UObject* child(UObject* parent,UObject* content,const wchar_t* =L"AddChild"){return UI::child(parent,content);}
UObject* canvas(UObject* parent,Layout::Rect rect){auto p=widget(L"/Script/UMG.CanvasPanel");UI::place(parent,p,rect);return p;}
UObject* label(UObject* parent,const std::wstring& value,Layout::Rect rect,double size=26,UI::Color color=UI::white){
    auto p=widget(L"/Script/UMG.TextBlock");UI::font(p,fontStyle.get(),size);UI::wrap(p,rect.w);UI::tint(p,color,true);setText(p,L"SetText",value);UI::place(parent,p,rect);
    setNumber(p,L"SetVisibility",L"InVisibility",4);return p;
}
UObject* image(UObject* parent,UObject* texture,Layout::Rect rect,UI::Color color={1,1,1,1}){
    auto p=widget(L"/Script/UMG.Image");UI::texture(p,texture);UI::tint(p,color);UI::place(parent,p,rect);setNumber(p,L"SetVisibility",L"InVisibility",4);return p;
}
UObject* plate(UObject* parent,Layout::Rect rect,UI::Color color){auto p=widget(L"/Script/UMG.Image");UI::solid(p,color);UI::place(parent,p,rect);setNumber(p,L"SetVisibility",L"InVisibility",4);return p;}
UObject* itemIcon(UObject* item){
    if(!item)return nullptr;auto it=itemIcons.find(item);if(it!=itemIcons.end()&&it->second.first.matches(item)&&(!it->second.second.address||it->second.second))return it->second.second.get();
    auto p=property(item,L"ItemImage");UObject* result{};
    if(p&&p->IsA<FSoftObjectProperty>()){
        Call c(find(L"/Script/Engine.Default__KismetSystemLibrary"),L"LoadAsset_Blocking");auto dst=c.field(L"Asset");
        if(dst&&dst->IsA<FSoftObjectProperty>()&&dst->GetSize()==p->GetSize()){dst->CopyCompleteValue(c.value(L"Asset"),p->ContainerPtrToValuePtr<void>(item));c.invoke();result=c.resultObject();}
    }else if(p&&p->IsA<FObjectProperty>())result=readObject(p,item);
    itemIcons[item]={Ref(item),Ref(result)};return result;
}
const Look* lookup(const Choice& c){if(c.mode==Choice::Mode::Look)for(const auto& look:runtime.looks)if(look.row==c.row)return &look;return nullptr;}
std::wstring caption(const Choice& c){if(c.mode==Choice::Mode::Original)return L"Original look";if(c.mode==Choice::Mode::Hidden)return L"Hidden";auto look=lookup(c);return look?look->label:c.row;}
UObject* choiceIcon(const Choice& c,unsigned category){auto look=lookup(c);auto icon=look?itemIcon(look->item.get()):nullptr;return icon?icon:categoryIcons[category].get();}
UObject* rarityFill(const Choice& choice){
    auto look=lookup(choice);auto item=look?look->item.get():nullptr;auto p=property(item,L"ItemRarity");if(!p||!p->IsA<FEnumProperty>())return nullptr;
    auto value=integer(p,item);auto type=static_cast<FEnumProperty*>(p)->GetEnum();if(!type)return nullptr;
    for(auto pair:type->ForEachName())if(pair.Value==value){auto name=pair.Key.ToString();auto split=name.rfind(L"::");if(split!=std::wstring::npos)name=name.substr(split+2);
        // These are the game's named inventory rarity fills. Unknown future
        // rarities retain the neutral frame instead of inventing a color.
        if(name!=L"Junk"&&name!=L"Common"&&name!=L"Superior"&&name!=L"Master"&&name!=L"Unique"&&name!=L"Catalyst")return nullptr;
        auto it=rarityTextures.find(name);if(it!=rarityTextures.end()&&(!it->second.address||it->second))return it->second.get();
        auto leaf=L"T_Item_Grid_Fill_"+name;auto path=L"/Game/_Dawnwalker/UI/_Unified/GameHub/Inventory/Atlas/Frames/"+leaf+L"."+leaf;
        auto result=asset(path.c_str());rarityTextures[name]=Ref(result);return result;
    }return nullptr;
}
void button(UObject* parent,const std::wstring& text,std::function<void()> callback,Layout::Rect rect,bool control=false,int category=-1,UObject* icon=nullptr,UObject* backing=nullptr){
    auto p=widget(L"/Script/UMG.Button");UI::buttonStyle(p,gridFrame.get(),activeFrame.get(),!control);UI::place(parent,p,rect);
    auto body=widget(L"/Script/UMG.CanvasPanel");auto slot=child(p,body);UI::fill(slot);
    Call padding(slot,L"SetPadding");for(auto side:{L"Left",L"Top",L"Right",L"Bottom"})UI::arg(padding,L"InPadding").at(side).num(0);padding.invoke();
    UObject* t{};if(backing){image(body,backing,{2,2,rect.w-4,rect.h-4});image(body,gridFrame.get(),{0,0,rect.w,rect.h});}
    if(icon)image(body,icon,{8,8,rect.w-16,rect.h-16});
    else {t=label(body,text,{8,control?8.:18.,rect.w-16,rect.h-10},control?24:28);setNumber(t,L"SetJustification",L"InJustification",1);}
    auto highlight=control?(category>=0&&category<5?image(body,categoryFrame.get(),{0,0,rect.w,rect.h},UI::gold):plate(body,{4,rect.h-3,rect.w-8,2},UI::gold)):image(body,activeFrame.get(),{0,0,rect.w,rect.h},UI::gold);
    UI::visible(highlight,false);setText(p,L"SetToolTipText",text);
    (control?controls:buttons).push_back({Ref(p),Ref(t),Ref(highlight),std::move(callback),category});
}
void focusPaint(){
    if(focus>=buttons.size())return;
    if(!popup&&runtime.model.page*Layout::pageSize+buttons.size()>pageChoices.size())return;
    for(unsigned i=0;i<buttons.size();++i){bool worn=!popup&&runtime.model.sets[runtime.model.displayedSet][runtime.model.category]==pageChoices[runtime.model.page*Layout::pageSize+i];
        auto h=buttons[i].highlight.get();UI::visible(h,i==focus||worn);if(h)UI::tint(h,worn?UI::gold:UI::white);}
    if(!popup)setText(footer.get(),L"SetText",caption(pageChoices[runtime.model.page*Layout::pageSize+focus]));shownFocus=focus;
}
void updateSummary(){
    for(unsigned i=0;i<5;++i){auto& choice=runtime.model.sets[runtime.model.displayedSet][i];if(auto p=summaryNames[i].get())setText(p,L"SetText",caption(choice));if(auto p=summaryImages[i].get()){UI::texture(p,choiceIcon(choice,i));UI::tint(p,choice.mode==Choice::Mode::Hidden?UI::muted:UI::white);}}
    for(auto& b:controls)if(b.category>=0&&b.category<5)UI::visible(b.highlight.get(),unsigned(b.category)==runtime.model.category);
    setText(setLabel.get(),L"SetText",runtime.model.displayedSet?L"NIGHT OUTFIT":L"DAY OUTFIT");shownRevision=runtime.model.revision;focusPaint();
}
void inputHints(){
    for(auto& c:controls)if(c.label){const wchar_t* value=nullptr;
        switch(c.category){
        case 10:value=controllerInput?(runtime.model.displayedSet?L"Night set  [View]":L"Day set  [View]"):(runtime.model.displayedSet?L"Night set  [Tab]":L"Day set  [Tab]");break;
        case 11:value=controllerInput?(runtime.model.allLooks?L"[R3]  Collected":L"[R3]  All looks"):(runtime.model.allLooks?L"[Y]  Collected":L"[Y]  All looks");break;
        case 12:value=controllerInput?L"[X]  Save outfit":L"[T]  Save outfit";break;
        case 13:value=controllerInput?L"[Y]  Load outfit":L"[S]  Load outfit";break;
        case 14:value=controllerInput?L"Hold X  Reset":L"[R]  Reset set";break;
        case 15:value=controllerInput?L"[B]  Back":L"[Esc]  Back";break;
        case 16:value=controllerInput?L"[L3]  Hide slot":L"[F]  Hide slot";break;
        case 17:value=controllerInput?L"LT":L"A";break;
        case 18:value=controllerInput?L"RT":L"D";break;
        case 100:value=controllerInput?L"[B]  Cancel":L"[Esc]  Cancel";break;
        }
        if(value)setText(c.label.get(),L"SetText",value);
        if(c.category==16)setNumber(c.object.get(),L"SetIsEnabled",L"bInIsEnabled",hideAllowed(static_cast<Slot>(runtime.model.category)));
    }paintHints=false;
}
UObject* createPage(){
    // HubSwitcher accepts UWidget children. A native panel avoids trying to
    // instantiate the abstract DWActivatableWidget class. The real hub keeps
    // ownership of input, pause and the widget tree.
    tree=Ref(object(runtime.hub.get(),L"WidgetTree"));
    return tree?construct(L"/Script/UMG.Overlay",tree.get()):nullptr;
}
void preview(){
    if(runtime.doll)return;auto p=runtime.player.get();if(!p)return;
    auto cls=asset(L"/Game/_Dawnwalker/UI/_Unified/GameHub/Inventory/Doll/BP_RenderDoll2.BP_RenderDoll2_C");if(!cls)return;
    auto library=find(L"/Script/Engine.Default__GameplayStatics");Call transform(p,L"GetTransform");if(!transform)return;transform.invoke();auto source=transform.fn->GetReturnProperty();
    Call spawn(library,L"BeginDeferredActorSpawnFromClass");if(!spawn)return;
    spawn.obj(L"WorldContextObject",p).obj(L"ActorClass",cls).num(L"CollisionHandlingOverride",1);
    if(spawn.field(L"TransformScaleMethod"))spawn.num(L"TransformScaleMethod",1);
    auto destination=spawn.field(L"SpawnTransform");if(!destination||!source||destination->GetSize()!=source->GetSize())return;destination->CopyCompleteValue(spawn.value(L"SpawnTransform"),source->ContainerPtrToValuePtr<void>(transform.data()));spawn.invoke();auto doll=spawn.resultObject();if(!doll)return;
    runtime.doll=Ref(doll);
    try{
        assignObject(property(doll,L"TargetInventory"),doll,runtime.inventory.get());Call app(runtime.appearance.get(),L"GetCurrentAppearance");app.invoke();assignObject(property(doll,L"AppearanceToApply"),doll,app.resultObject());
        Call finish(library,L"FinishSpawningActor");finish.obj(L"Actor",doll);auto dst=finish.field(L"SpawnTransform");if(!dst||dst->GetSize()!=source->GetSize())throw std::runtime_error("Preview transform changed");dst->CopyCompleteValue(finish.value(L"SpawnTransform"),source->ContainerPtrToValuePtr<void>(transform.data()));if(finish.field(L"TransformScaleMethod"))finish.num(L"TransformScaleMethod",1);finish.invoke();setDoll(doll);
        if(auto image=previewImage.get()){Call brush(image,L"SetBrushFromMaterial");brush.obj(L"Material",asset(L"/Game/_Dawnwalker/UI/_Unified/GameHub/Inventory/Doll/M_UI_RenderDoll.M_UI_RenderDoll")).invoke();}
    }catch(...){call(doll,L"K2_DestroyActor");runtime.doll={};warn(L"Character preview unavailable; look selection remains available.");}
}
void stopPreview(){if(auto p=runtime.doll.get())call(p,L"K2_DestroyActor");runtime.doll={};runtime.dollAppearance={};}
void rebuildList(){
    filtered.clear();pageChoices.clear();for(const auto& look:runtime.looks)if(static_cast<unsigned>(look.slot)==runtime.model.category&&(runtime.model.allLooks||runtime.model.collected.contains(look.row)))filtered.push_back(look);
    pageChoices.push_back({});if(hideAllowed(static_cast<Slot>(runtime.model.category)))pageChoices.push_back({Choice::Mode::Hidden,{}});
    for(auto& look:filtered)pageChoices.push_back({Choice::Mode::Look,look.row});
    runtime.model.page=std::min(runtime.model.page,Layout::pages(static_cast<unsigned>(pageChoices.size()))-1);focus=0;
}
void draw(){
    auto panel=popup?modalGrid.get():grid.get();if(!panel)return;call(panel,L"ClearChildren");buttons.clear();tileCursor=0;rebuildList();shownFocus=~0u;
    setText(title.get(),L"SetText",slotLabels[runtime.model.category]);
    setText(countLabel.get(),L"SetText",runtime.catalogReady?std::to_wstring(filtered.size())+(runtime.model.allLooks?L" looks (all)":L" looks (collected)"):L"Loading looks...");
    setText(pageLabel.get(),L"SetText",std::to_wstring(runtime.model.page+1)+L" / "+std::to_wstring(Layout::pages(static_cast<unsigned>(pageChoices.size()))));
    setText(footer.get(),L"SetText",L"");UI::visible(modal.get(),popup!=0);
    setText(modalTitle.get(),L"SetText",popup==1?L"SAVE OUTFIT":L"LOAD OUTFIT");
    setText(modalHelp.get(),L"SetText",popup==1?L"Choose a slot to save the displayed outfit.":L"Choose a saved outfit to wear.");
    for(auto& c:controls)if(c.category==10&&c.label)setText(c.label.get(),L"SetText",runtime.model.displayedSet?L"Night set  [Tab]":L"Day set  [Tab]");
    updateSummary();paintHints=true;redraw=false;
}
void addTiles(){
    auto panel=popup?modalGrid.get():grid.get();if(!panel)return;
    unsigned total=popup?3:Layout::count(static_cast<unsigned>(pageChoices.size()),runtime.model.page);
    // At most two visible entries per continuation. Textures are resolved only
    // for those entries and retained by their brushes, never for the full catalog.
    bool added=tileCursor<total;for(unsigned n=0;n<2&&tileCursor<total;++n,++tileCursor){
        auto index=tileCursor;
        if(popup){auto name=L"Outfit "+std::to_wstring(index+1)+(runtime.model.presets[index]?L"     Saved":L"     Empty");button(panel,name,[index]{bool saving=popup==1;if(!saving&&!runtime.model.presets[index])return;bool change=saving?runtime.model.save(index):runtime.model.load(index);if(change){runtime.dirty=true;if(!saving)requestRefresh(runtime.model.displayedSet==runtime.activeSet,true);}popup=0;redraw=true;},{0,double(index)*94,692,80});}
        else {auto absolute=runtime.model.page*Layout::pageSize+index;auto choice=pageChoices[absolute];auto icon=choiceIcon(choice,runtime.model.category);
            button(panel,caption(choice),[choice]{selectLook(static_cast<Slot>(runtime.model.category),choice);},Layout::tile(index),false,-1,icon,rarityFill(choice));
            if(choice.mode!=Choice::Mode::Look){Call content(buttons.back().object.get(),L"GetContent");content.invoke();if(auto body=content.resultObject()){plate(body,{2,67,90,25},UI::ink);auto t=label(body,choice.mode==Choice::Mode::Hidden?L"HIDE":L"ORIGINAL",{0,67,94,25},17);setNumber(t,L"SetJustification",L"InJustification",1);}}
        }
    }
    if(added||shownFocus!=focus)focusPaint();
    if(added&&tileCursor==total&&logging)trace(L"Wardrobe view ready: visible="+std::to_wstring(total)+L", cached item icons="+std::to_wstring(itemIcons.size())+L", dialog="+std::to_wstring(popup));
}
void build(){
    auto p=page.get();if(!p||!tree)return;
    asset(L"/Game/_Dawnwalker/UI/_Unified/TextStyles/DTS_Common_White.DTS_Common_White_C");
    fontStyle=Ref(find(L"/Game/_Dawnwalker/UI/_Unified/TextStyles/DTS_Common_White.Default__DTS_Common_White_C"));
    if(!fontStyle)throw std::runtime_error("The game's Wardrobe text style is unavailable");
    gridFrame=Ref(asset(L"/Game/_Dawnwalker/UI/_Unified/GameHub/Inventory/Atlas/Frames/T_Item_Grid_Background.T_Item_Grid_Background"));
    activeFrame=Ref(asset(L"/Game/_Dawnwalker/UI/_Unified/GameHub/Inventory/Atlas/Frames/T_Item_Grid_Active.T_Item_Grid_Active"));
    categoryFrame=Ref(asset(L"/Game/_Dawnwalker/UI/_Unified/SharedTextures/General/Frames/T_Icon_Category_ActiveWhite.T_Icon_Category_ActiveWhite"));
    const wchar_t* iconNames[]={L"T_Icon_ItemFilter_Armor",L"T_Icon_Category_Trousers",L"T_Icon_Category_Gauntlets",L"T_Icon_Category_Boots",L"T_Icon_ItemFilter_Weapon"};
    for(unsigned i=0;i<5;++i){auto path=std::wstring(frames)+iconNames[i]+L"."+iconNames[i];categoryIcons[i]=Ref(asset(path.c_str()));}
    // One aspect-preserving design surface fills the hub's available content
    // area at every resolution. Canvas slots receive explicit rectangles.
    auto scale=widget(L"/Script/UMG.ScaleBox");UI::fill(child(p,scale));setNumber(scale,L"SetStretch",L"InStretch",2);
    auto size=widget(L"/Script/UMG.SizeBox");child(scale,size);setNumber(size,L"SetWidthOverride",L"InWidthOverride",Layout::width);setNumber(size,L"SetHeightOverride",L"InHeightOverride",Layout::height);
    auto stage=widget(L"/Script/UMG.CanvasPanel");UI::fill(child(size,stage));root=Ref(stage);setNumber(stage,L"SetClipping",L"InClipping",1);
    plate(stage,{0,0,Layout::width,Layout::height},UI::ink);
    image(stage,asset(L"/Game/_Dawnwalker/UI/_Unified/SharedTextures/Backgrounds/T_Generic_Line_Background.T_Generic_Line_Background"),{0,0,Layout::width,Layout::height},{.18,.19,.20,.3});
    previewImage=Ref(image(stage,nullptr,Layout::doll));
    title=Ref(label(stage,L"Armour",{128,130,360,44},28));
    countLabel=Ref(label(stage,L"Loading looks...",{468,130,296,44},26));setNumber(countLabel.get(),L"SetJustification",L"InJustification",2);
    plate(stage,{128,174,624,1},UI::muted);grid=Ref(canvas(stage,Layout::catalog));
    footer=Ref(label(stage,L"",{128,822,636,70},26));
    pageLabel=Ref(label(stage,L"1 / 1",{385,901,110,40},24,UI::muted));setNumber(pageLabel.get(),L"SetJustification",L"InJustification",1);
    controls.clear();pendingControls.clear();
    auto action=[&](const wchar_t* name,Layout::Rect rect,std::function<void()> fn,int category=-1){pendingControls.push_back({Ref(stage),name,std::move(fn),rect,category});};
    action(L"A",{120,65,42,48},[]{runtime.model.category=(runtime.model.category+4)%5;runtime.model.page=0;redraw=true;},17);
    for(unsigned i=0;i<5;++i)action(slotLabels[i],{178+double(i)*78,48,68,68},[i]{runtime.model.category=i;runtime.model.page=0;redraw=true;},i);
    action(L"D",{578,65,42,48},[]{runtime.model.category=(runtime.model.category+1)%5;runtime.model.page=0;redraw=true;},18);
    label(stage,L"WARDROBE PREVIEW",{1600,52,246,40},25,UI::muted);
    setLabel=Ref(label(stage,L"DAY OUTFIT",{1600,98,240,38},24,UI::gold));
    action(L"Day set  [Tab]",{1600,142,240,46},[]{runtime.model.switchSet(1-runtime.model.displayedSet);requestRefresh(false,true);redraw=true;},10);
    for(unsigned i=0;i<5;++i){double y=216+i*120;image(stage,gridFrame.get(),{1600,y,72,72});summaryImages[i]=Ref(image(stage,categoryIcons[i].get(),{1604,y+4,64,64}));label(stage,slotLabels[i],{1684,y-2,184,44},22,UI::muted);summaryNames[i]=Ref(label(stage,L"Original look",{1684,y+34,184,82},25));}
    action(L"<  Previous",{128,895,202,46},[]{if(runtime.model.page){--runtime.model.page;redraw=true;}});
    action(L"Next  >",{562,895,202,46},[]{if((runtime.model.page+1)*Layout::pageSize<pageChoices.size()){++runtime.model.page;redraw=true;}});
    plate(stage,{64,951,1792,1},{.19,.19,.16,.7});
    action(L"[Esc]  Back",{64,962,200,38},[]{closeMenu();},15);
    action(L"[F]  Hide slot",{790,962,198,38},[]{selectLook(static_cast<Slot>(runtime.model.category),{Choice::Mode::Hidden,{}});},16);
    action(L"[Y]  All looks",{990,962,205,38},[]{runtime.model.allLooks=!runtime.model.allLooks;runtime.dirty=true;runtime.model.page=0;redraw=true;},11);
    action(L"[T]  Save outfit",{1204,962,214,38},[]{popup=1;redraw=true;},12);
    action(L"[S]  Load outfit",{1428,962,220,38},[]{popup=2;redraw=true;},13);
    action(L"[R]  Reset set",{1658,962,204,38},[]{if(runtime.model.reset()){runtime.dirty=true;requestRefresh(runtime.model.displayedSet==runtime.activeSet,true);}redraw=true;},14);
    // The modal is a separate top layer. The catalog stays visible underneath;
    // input dispatch excludes all background controls while a dialog is open.
    auto overlay=canvas(stage,{0,0,1920,1000});modal=Ref(overlay);plate(overlay,{0,0,1920,1000},{0,0,0,.72});plate(overlay,Layout::dialog,{.022,.028,.032,1});
    plate(overlay,{550,242,820,2},UI::gold);plate(overlay,{550,756,820,2},UI::gold);
    modalTitle=Ref(label(overlay,L"SAVE OUTFIT",{614,274,692,50},36));modalHelp=Ref(label(overlay,L"",{614,330,692,46},26,UI::muted));modalGrid=Ref(canvas(overlay,{614,394,692,282}));
    pendingControls.push_back({Ref(overlay),L"[Esc]  Cancel",[]{popup=0;redraw=true;},{866,696,188,44},100});UI::visible(overlay,false);
    built=true;redraw=true;shownRevision=~uint64_t{};
}
void show(){
    wantsOpen=false;
    if(!runtime.settings.enabled||!runtime.playerReady)return;
    auto hub=runtime.hub.get();auto switcher=object(hub,L"HubSwitcher");if(!switcher)throw std::runtime_error("HubSwitcher unavailable");
    if(!page){page=Ref(createPage());if(!page)throw std::runtime_error("Wardrobe page could not be created in the hub widget tree");build();child(switcher,page.get());}
    Call select(switcher,L"SetActiveWidget");select.obj(L"Widget",page.get()).invoke();
    if(auto nav=navbar.get()){Call selectTab(nav,L"SelectTab");if(selectTab){tag(selectTab.field(L"InTabTag"),selectTab.data(),tagName);selectTab.invoke();}}
    runtime.menuOpen=true;runtime.model.switchSet(runtime.activeSet);wantsOpen=false;popup=0;redraw=true;beginCatalog();preview();if(logging)trace(L"Wardrobe page opened.");
    for(int i=0;i<256;++i)previousKeys[i]=(GetAsyncKeyState(i)&0x8000)!=0;
    GetCursorPos(&previousMouse);xPressed=0;
    XINPUT_STATE state{};previousPad=XInputGetState(0,&state)==ERROR_SUCCESS?state.Gamepad.wButtons:0;
}
bool inject(){
    auto hub=runtime.hub.get();if(!hub)return false;auto nav=object(hub,L"NavBar");if(!nav)return false;navbar=Ref(nav);
    showFunction=Ref(function(hub,L"Show Tab"));activated.bind(hub,L"BP_OnActivated");deactivated.bind(hub,L"BP_OnDeactivated");rebuilt.bind(nav,L"Rebuild Buttons");if(!showFunction)return false;
    Call c(nav,L"CreateTabButton");if(!c)return false;auto row=c.field(L"HubTabRow");if(!row||!row->IsA<FStructProperty>())return false;
    auto st=static_cast<FStructProperty*>(row)->GetStruct();void* data=c.value(L"HubTabRow");tag(property(st,L"TabTag"),data,tagName);assignText(property(st,L"DisplayName"),data,L"WARDROBE");
    auto group=object(nav,L"Tab Button Group");if(!group)return false;int64_t before=-1;{Call count(group,L"GetButtonCount");if(count){count.invoke();before=count.resultInteger();}}
    auto b=tabButton.get();if(!b){c.invoke();b=readObject(c.field(L"Out Button"),c.data());if(!b)b=c.resultObject();if(!b)return false;tabButton=Ref(b);}
    if(group&&before>=0){Call count(group,L"GetButtonCount");count.invoke();if(count.resultInteger()==before){Call add(group,L"AddWidget");add.obj(L"InWidget",b).invoke();}}
    if(logging)trace(L"Wardrobe tab added to the hub.");
    return true;
}
void move(int delta){if(buttons.empty())return;auto total=popup?3:Layout::count(static_cast<unsigned>(pageChoices.size()),runtime.model.page);if(buttons.size()<total)return;int next=static_cast<int>(focus)+delta;if(!popup&&next<0&&runtime.model.page){--runtime.model.page;redraw=true;return;}if(!popup&&next>=static_cast<int>(buttons.size())&&(runtime.model.page+1)*Layout::pageSize<pageChoices.size()){++runtime.model.page;redraw=true;return;}focus=static_cast<unsigned>((next%static_cast<int>(buttons.size())+static_cast<int>(buttons.size()))%static_cast<int>(buttons.size()));focusPaint();}
}
void initializeMenu(){hubGraph=Ref(find(L"/Game/_Dawnwalker/UI/_Unified/GameHub/WBP_Window_GameHub.WBP_Window_GameHub_C:ExecuteUbergraph_WBP_Window_GameHub"));}
bool menuPending(){return injection.pending||wantsOpen;}
void menuHub(UObject* h){
    if(runtime.hub.matches(h)&&navbar.matches(object(h,L"NavBar"))&&tabButton){
        auto p=property(tabButton.get(),L"ButtonTag");if(p&&p->IsA<FStructProperty>()){auto type=static_cast<FStructProperty*>(p)->GetStruct();if(text(property(type,L"TagName"),p->ContainerPtrToValuePtr<void>(tabButton.get()))==tagName)return;}
    }
    bool open=wantsOpen;stopPreview();resetMenu();wantsOpen=open;runtime.hub=Ref(h);hubGraph=Ref(function(h,L"ExecuteUbergraph_WBP_Window_GameHub"));injection.request(runtime.now);
    if(logging)trace(L"Game hub detected; preparing Wardrobe tab.");
}
void menuRedraw(){redraw=true;}
void menuScriptPre(UObject* owner,UFunction* fn,void* params,Hook::TCallbackIterationData<void>& info){
    if(!runtime.hub.matches(owner)||!showFunction.matches(fn))return;
    if(ourTag(fn,params)){info.PreventOriginalFunctionCall();if(runtime.settings.enabled){if(runtime.playerReady)wantsOpen=true;else requestOpen();}}
    else if(runtime.menuOpen){runtime.menuOpen=false;stopPreview();writeStore();}
}
void menuScriptPost(UObject* owner,UFunction* fn,void* params){
    // Parameterless Blueprint events can skip BP_OnActivated and run the
    // compiled event graph directly. Its first execution discovers the hub.
    if(fn&&owner&&(hubGraph.address==fn||!hubGraph.address)){
        static const FName graphName(L"ExecuteUbergraph_WBP_Window_GameHub"),hubClass(L"WBP_Window_GameHub_C");
        if(fn->GetFName()==graphName&&owner->GetClassPrivate()->GetFName()==hubClass){
            hubGraph=Ref(fn);if(!runtime.hub.matches(owner)||!navbar)queueHub(owner);
        }
    }
    if(runtime.hub.matches(owner)){
        if(activated.matches(fn,params))queueHub(owner);
        if(deactivated.matches(fn,params)){runtime.menuOpen=false;stopPreview();writeStore();}
    }
    if(navbar.matches(owner)&&rebuilt.matches(fn,params)){tabButton={};injection.cancel();injection.request(runtime.now);}
}
void openMenu(){
    if(!runtime.player)return;wantsOpen=true;
    // The normal hub owns pause/input and navigation. Its creation event then
    // injects the Wardrobe button and selects this page.
    // UIManagerSubsystem belongs to the local player. The game's own helper
    // resolves that owner and its active frontend; a game-instance lookup fails.
    Call frontendCall(find(L"/Script/DogwoodUI.Default__UIFrontend"),L"GetFrontend");
    frontendCall.obj(L"WorldContextObject",runtime.player.get()).invoke();auto frontend=frontendCall.resultObject();
    if(!frontend)throw std::runtime_error("UIFrontend.GetFrontend returned no active frontend");
    auto layer=object(frontend,L"GameMenuLayer");if(!layer)throw std::runtime_error("Active UIFrontend has no GameMenuLayer");
    auto cls=asset(L"/Game/_Dawnwalker/UI/_Unified/GameHub/WBP_Window_GameHub.WBP_Window_GameHub_C");if(!cls)throw std::runtime_error("Game hub widget class could not be loaded");
    {
        Call current(layer,L"GetActiveWidget");current.invoke();auto h=current.resultObject();
        if(h&&h->GetClassPrivate()==cls){menuHub(h);if(tabButton)show();return;}
        Call c(layer,L"BP_AddWidget");c.obj(L"ActivatableWidgetClass",cls).invoke();auto created=c.resultObject();if(!created)throw std::runtime_error("The game menu layer did not create a hub");menuHub(created);
    }
}
void closeMenu(){
    wantsOpen=false;popup=0;if(!runtime.menuOpen){stopPreview();return;}runtime.menuOpen=false;stopPreview();
    if(auto hub=runtime.hub.get())call(hub,L"DeactivateWidget");writeStore();
}
void resetMenu(){
    auto oldPage=page;
    runtime.menuOpen=false;wantsOpen=false;popup=0;injection.cancel();page={};tree={};root={};grid={};title={};footer={};navbar={};tabButton={};previewImage={};showFunction={};hubGraph={};activated={};deactivated={};rebuilt={};buttons.clear();controls.clear();pendingControls.clear();filtered.clear();pageChoices.clear();built=false;countLabel={};pageLabel={};setLabel={};modal={};modalGrid={};modalTitle={};modalHelp={};fontStyle={};gridFrame={};activeFrame={};categoryFrame={};categoryIcons={};summaryNames={};summaryImages={};itemIcons.clear();rarityTextures.clear();xPressed=0;
    if(auto p=oldPage.get())try{call(p,L"RemoveFromParent");}catch(const std::exception& e){failure(L"Removing Wardrobe page",e);}
}
void stepMenu(){
    if(injection.ready(runtime.now)){auto ok=inject();injection.finish(ok,runtime.now);if(!ok&&!injection.pending){wantsOpen=false;warn(L"Wardrobe tab could not be added to this hub layout.");}return;}
    if(wantsOpen&&!injection.pending){if(!tabButton)throw std::runtime_error("Wardrobe tab was removed before opening");show();}
    if(!runtime.menuOpen)return;if(!page||!runtime.hub){runtime.menuOpen=false;stopPreview();resetMenu();return;}
    // Keep the same render doll for the entire visit. Appearance changes are
    // applied in place; saving, collection learning and filtering do not spawn it.
    if(redraw)draw();
    if(!pendingControls.empty()){for(unsigned i=0;i<2&&!pendingControls.empty();++i){auto control=std::move(pendingControls.front());pendingControls.pop_front();button(control.parent.get(),control.caption,std::move(control.click),control.rect,true,control.category,control.category>=0&&control.category<5?categoryIcons[control.category].get():nullptr);if(control.category>=0&&control.category<5)UI::visible(controls.back().highlight.get(),unsigned(control.category)==runtime.model.category);}paintHints=true;}
    else addTiles(); // One allocation budget shared by controls and choices.
    if(shownRevision!=runtime.model.revision)updateSummary();
    DWORD foreground{};GetWindowThreadProcessId(GetForegroundWindow(),&foreground);if(foreground!=GetCurrentProcessId())return;
    pollKeys();
    XINPUT_STATE state{};WORD pad{};BYTE left{},right{};if(XInputGetState(0,&state)==ERROR_SUCCESS){pad=state.Gamepad.wButtons;left=state.Gamepad.bLeftTrigger;right=state.Gamepad.bRightTrigger;}
    WORD edges=pad&~previousPad;
    bool padActivity=edges||(left>128&&previousLeft<=128)||(right>128&&previousRight<=128);
    if(padActivity&&!controllerInput){controllerInput=true;paintHints=true;}
    if(paintHints)inputHints();
    bool escape=pressed(VK_ESCAPE)||(edges&XINPUT_GAMEPAD_B);if(escape){if(popup){popup=0;redraw=true;}else closeMenu();previousPad=pad;return;}
    if(!popup){
    if(pressed('A')||(left>128&&previousLeft<=128)){runtime.model.category=(runtime.model.category+4)%5;runtime.model.page=0;redraw=true;}
    if(pressed('D')||(right>128&&previousRight<=128)){runtime.model.category=(runtime.model.category+1)%5;runtime.model.page=0;redraw=true;}
    if(pressed(VK_PRIOR)&&runtime.model.page){--runtime.model.page;redraw=true;}
    if(pressed(VK_NEXT)){++runtime.model.page;redraw=true;}
    if(pressed(VK_TAB)||(edges&XINPUT_GAMEPAD_BACK)){runtime.model.switchSet(1-runtime.model.displayedSet);requestRefresh(false,true);redraw=true;}
    if(pressed('Y')||(edges&XINPUT_GAMEPAD_RIGHT_THUMB)){runtime.model.allLooks=!runtime.model.allLooks;runtime.dirty=true;runtime.model.page=0;redraw=true;}
    if(pressed('F')||(edges&XINPUT_GAMEPAD_LEFT_THUMB))selectLook(static_cast<Slot>(runtime.model.category),{Choice::Mode::Hidden,{}});
    if(pressed('T')){popup=1;redraw=true;}
    if(pressed('S')||(edges&XINPUT_GAMEPAD_Y)){popup=2;redraw=true;}
    if(edges&XINPUT_GAMEPAD_X)xPressed=runtime.now;
    bool heldReset=xPressed&&(pad&XINPUT_GAMEPAD_X)&&runtime.now-xPressed>=600;
    if(pressed('R')||heldReset){if(runtime.model.reset()){runtime.dirty=true;requestRefresh(runtime.model.displayedSet==runtime.activeSet,true);}xPressed=0;popup=0;redraw=true;}
    if(xPressed&&!(pad&XINPUT_GAMEPAD_X)){popup=1;redraw=true;xPressed=0;}
    }
    if(redraw){previousPad=pad;previousLeft=left;previousRight=right;return;}
    if(pressed(VK_LEFT)||(edges&XINPUT_GAMEPAD_DPAD_LEFT))move(-1);
    if(pressed(VK_RIGHT)||(edges&XINPUT_GAMEPAD_DPAD_RIGHT))move(1);
    if(pressed(VK_UP)||(edges&XINPUT_GAMEPAD_DPAD_UP))move(popup?-1:-int(Layout::columns));
    if(pressed(VK_DOWN)||(edges&XINPUT_GAMEPAD_DPAD_DOWN))move(popup?1:int(Layout::columns));
    bool click=pressed(VK_LBUTTON);POINT mouse{};GetCursorPos(&mouse);
    if(controllerInput&&(click||mouse.x!=previousMouse.x||mouse.y!=previousMouse.y)){controllerInput=false;paintHints=true;}
    if(click){for(auto& control:controls)if((popup?control.category==100:control.category!=100))if(auto b=control.object.get()){Call hovered(b,L"IsHovered");hovered.invoke();if(hovered.resultInteger()){control.click();click=false;break;}}}
    if(!runtime.menuOpen)return;
    if(!redraw&&(click||mouse.x!=previousMouse.x||mouse.y!=previousMouse.y)){for(unsigned i=0;i<buttons.size();++i)if(auto b=buttons[i].object.get()){Call hovered(b,L"IsHovered");hovered.invoke();if(hovered.resultInteger()){if(focus!=i){focus=i;focusPaint();}if(click)buttons[i].click();break;}}previousMouse=mouse;}
    if((pressed(VK_RETURN)||(edges&XINPUT_GAMEPAD_A))&&!redraw&&focus<buttons.size())buttons[focus].click();
    previousPad=pad;previousLeft=left;previousRight=right;
}
}
