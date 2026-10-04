#include "Runtime.hpp"
#include <Unreal/Core/Windows/AllowWindowsPlatformTypes.hpp>
#include <Windows.h>
#include <Xinput.h>
#include <deque>
#include <optional>

namespace Wardrobe {
namespace {
constexpr auto tagName=L"UI.Menu.HUB.WardrobeTransmog";
Ref page,tree,root,grid,title,footer,navbar,tabButton,previewImage;
Ref showFunction,deactivateFunction,rebuildFunction;
Ref activateFunction;
Work injection;bool wantsOpen{},redraw{},built{};unsigned tileCursor{},popup{},focus{};
struct Button {Ref object,label;std::function<void()> click;};std::vector<Button> buttons;
std::vector<Button> controls;
struct Control {Ref parent;std::wstring caption;std::function<void()> click;};std::deque<Control> pendingControls;
std::vector<Look> filtered;std::vector<Choice> pageChoices;
std::array<bool,256> previousKeys{};WORD previousPad{};BYTE previousLeft{},previousRight{};POINT previousMouse{};uint64_t xPressed{};
bool pressed(int key){bool down=(GetAsyncKeyState(key)&0x8000)!=0;bool edge=down&&!previousKeys[key];previousKeys[key]=down;return edge;}
void tag(FProperty* p,void* base,const wchar_t* text){
    if(!p||!p->IsA<FStructProperty>())throw std::runtime_error("Expected gameplay tag");auto st=static_cast<FStructProperty*>(p)->GetStruct();auto field=property(st,L"TagName");assignText(field,p->ContainerPtrToValuePtr<void>(base),text);
}
bool ourTag(UFunction* fn,void* params){
    for(auto p:fn->ForEachProperty())if(p->IsA<FStructProperty>()&&(p->GetPropertyFlags()&CPF_Parm)){
        auto st=static_cast<FStructProperty*>(p)->GetStruct();auto n=property(st,L"TagName");if(n)return text(n,p->ContainerPtrToValuePtr<void>(params))==tagName;
    }return false;
}
UObject* widget(const wchar_t* cls){auto p=construct(cls,tree.get());if(!p)throw std::runtime_error("Widget class unavailable");return p;}
UObject* child(UObject* parent,UObject* content,const wchar_t* method=L"AddChild"){
    Call c(parent,method);c.obj(L"Content",content).invoke();return c.resultObject();
}
void fill(UObject* slot){if(!slot)return;Call c(slot,L"SetHorizontalAlignment");if(c)c.num(L"InHorizontalAlignment",0).invoke();}
UObject* label(UObject* parent,const std::wstring& value,float size=20){
    auto p=widget(L"/Script/UMG.TextBlock");setText(p,L"SetText",value);
    if(auto font=property(p,L"Font");font&&font->IsA<FStructProperty>()){
        auto st=static_cast<FStructProperty*>(font)->GetStruct();number(property(st,L"Size"),font->ContainerPtrToValuePtr<void>(p),size);
    }
    child(parent,p);return p;
}
UObject* sized(UObject* parent,float width,float height){auto p=widget(L"/Script/UMG.SizeBox");setNumber(p,L"SetWidthOverride",L"InWidthOverride",width);setNumber(p,L"SetHeightOverride",L"InHeightOverride",height);child(parent,p);return p;}
void button(UObject* parent,const std::wstring& text,std::function<void()> callback,bool control=false){
    auto size=sized(parent,control?170:220,52);auto p=widget(L"/Script/UMG.Button");child(size,p);auto t=label(p,text,control?17:19);(control?controls:buttons).push_back({Ref(p),Ref(t),std::move(callback)});
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
    unsigned last=pageChoices.empty()?0:static_cast<unsigned>((pageChoices.size()-1)/12);runtime.model.page=std::min(runtime.model.page,last);focus=0;
}
std::wstring choiceLabel(unsigned index){
    const auto& c=pageChoices[index];if(c.mode==Choice::Mode::Original)return L"Original look";if(c.mode==Choice::Mode::Hidden)return L"Hidden";
    for(auto& look:filtered)if(look.row==c.row)return look.label;return c.row;
}
void draw(){
    auto panel=grid.get();if(!panel)return;call(panel,L"ClearChildren");buttons.clear();tileCursor=0;rebuildList();
    auto name=std::wstring(L"WARDROBE  /  ")+slotLabels[runtime.model.category]+(runtime.model.displayedSet?L"  /  Night set":L"  /  Day set");setText(title.get(),L"SetText",name);
    std::wstring info=runtime.catalogReady?(runtime.model.allLooks?L"All looks":L"Collected looks"):L"Loading looks...";
    info+=L"  |  Page "+std::to_wstring(runtime.model.page+1)+L"  |  A/D: category  |  Page Up/Down: page\nEnter/A: wear  |  F/L3: hide  |  Y/R3: all looks  |  T/X: save  |  S/Y: load\nR/hold X: reset set  |  Tab: day/night preview  |  Esc/B: close";
    if(popup)info=popup==1?L"Save current outfit: choose slot 1, 2 or 3. Escape cancels.":L"Load outfit: choose slot 1, 2 or 3. Escape cancels.";
    setText(footer.get(),L"SetText",info);redraw=false;
}
void addTiles(){
    auto panel=grid.get();if(!panel)return;
    unsigned total=popup?3:std::min<unsigned>(12,static_cast<unsigned>(pageChoices.size())-runtime.model.page*12);
    // Two tiles per frame: six widget allocations, never the entire catalog.
    for(unsigned n=0;n<2&&tileCursor<total;++n,++tileCursor){
        auto index=tileCursor;
        if(popup){auto caption=L"Outfit "+std::to_wstring(index+1)+(runtime.model.presets[index]?L"  Saved":L"  Empty");button(panel,caption,[index]{bool saving=popup==1;bool change=saving?runtime.model.save(index):runtime.model.load(index);if(change){runtime.dirty=true;if(!saving)requestRefresh(runtime.model.displayedSet==runtime.activeSet,true);}popup=0;redraw=true;});}
        else {auto absolute=runtime.model.page*12+index;auto choice=pageChoices[absolute];button(panel,choiceLabel(absolute),[choice]{selectLook(static_cast<Slot>(runtime.model.category),choice);});}
    }
}
void build(){
    auto p=page.get();if(!p||!tree)return;
    auto vertical=widget(L"/Script/UMG.VerticalBox");root=Ref(vertical);auto slot=child(p,vertical);fill(slot);if(slot)setNumber(slot,L"SetVerticalAlignment",L"InVerticalAlignment",0);
    title=Ref(label(vertical,L"WARDROBE",30));
    controls.clear();pendingControls.clear();
    auto categoryBar=widget(L"/Script/UMG.HorizontalBox");child(vertical,categoryBar);
    for(unsigned i=0;i<5;++i)pendingControls.push_back({Ref(categoryBar),slotLabels[i],[i]{runtime.model.category=i;runtime.model.page=0;popup=0;redraw=true;}});
    auto actionBar=widget(L"/Script/UMG.HorizontalBox");child(vertical,actionBar);
    auto action=[&](const wchar_t* name,std::function<void()> click){pendingControls.push_back({Ref(actionBar),name,std::move(click)});};
    action(L"Day / Night",[]{runtime.model.switchSet(1-runtime.model.displayedSet);requestRefresh(false,true);redraw=true;});
    action(L"Collected / All",[]{runtime.model.allLooks=!runtime.model.allLooks;runtime.dirty=true;runtime.model.page=0;redraw=true;});
    action(L"Hide",[]{selectLook(static_cast<Slot>(runtime.model.category),{Choice::Mode::Hidden,{}});});
    action(L"Save Outfit",[]{popup=1;redraw=true;});action(L"Load Outfit",[]{popup=2;redraw=true;});
    action(L"Reset Set",[]{if(runtime.model.reset()){runtime.dirty=true;requestRefresh(runtime.model.displayedSet==runtime.activeSet,true);}popup=0;redraw=true;});
    action(L"Close",[]{closeMenu();});
    auto horizontal=widget(L"/Script/UMG.HorizontalBox");fill(child(vertical,horizontal));
    auto left=sized(horizontal,900,440);auto wrap=widget(L"/Script/UMG.WrapBox");child(left,wrap);grid=Ref(wrap);
    auto image=widget(L"/Script/UMG.Image");child(sized(horizontal,390,440),image);previewImage=Ref(image);
    auto pages=widget(L"/Script/UMG.HorizontalBox");child(vertical,pages);
    pendingControls.push_back({Ref(pages),L"Previous Page",[]{if(runtime.model.page){--runtime.model.page;redraw=true;}}});
    pendingControls.push_back({Ref(pages),L"Next Page",[]{if((runtime.model.page+1)*12<pageChoices.size()){++runtime.model.page;redraw=true;}}});
    footer=Ref(label(vertical,L"Loading looks...",18));built=true;redraw=true;
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
    XINPUT_STATE state{};previousPad=XInputGetState(0,&state)==ERROR_SUCCESS?state.Gamepad.wButtons:0;
}
bool inject(){
    auto hub=runtime.hub.get();if(!hub)return false;auto nav=object(hub,L"NavBar");if(!nav)return false;navbar=Ref(nav);
    showFunction=Ref(function(hub,L"Show Tab"));deactivateFunction=Ref(function(hub,L"BP_OnDeactivated"));rebuildFunction=Ref(function(nav,L"Rebuild Buttons"));if(!showFunction)return false;
    Call c(nav,L"CreateTabButton");if(!c)return false;auto row=c.field(L"HubTabRow");if(!row||!row->IsA<FStructProperty>())return false;
    auto st=static_cast<FStructProperty*>(row)->GetStruct();void* data=c.value(L"HubTabRow");tag(property(st,L"TabTag"),data,tagName);assignText(property(st,L"DisplayName"),data,L"WARDROBE");
    auto group=object(nav,L"Tab Button Group");if(!group)return false;int64_t before=-1;{Call count(group,L"GetButtonCount");if(count){count.invoke();before=count.resultInteger();}}
    auto b=tabButton.get();if(!b){c.invoke();b=readObject(c.field(L"Out Button"),c.data());if(!b)b=c.resultObject();if(!b)return false;tabButton=Ref(b);}
    if(group&&before>=0){Call count(group,L"GetButtonCount");count.invoke();if(count.resultInteger()==before){Call add(group,L"AddWidget");add.obj(L"InWidget",b).invoke();}}
    if(logging)trace(L"Wardrobe tab added to the hub.");
    return true;
}
void move(int delta){if(buttons.empty())return;int next=static_cast<int>(focus)+delta;if(!popup&&next<0&&runtime.model.page){--runtime.model.page;redraw=true;return;}if(!popup&&next>=static_cast<int>(buttons.size())&&(runtime.model.page+1)*12<pageChoices.size()){++runtime.model.page;redraw=true;return;}focus=static_cast<unsigned>((next+static_cast<int>(buttons.size()))%static_cast<int>(buttons.size()));for(unsigned i=0;i<buttons.size();++i)if(auto b=buttons[i].object.get())setNumber(b,L"SetRenderOpacity",L"InOpacity",i==focus?1:.65);}
}
void initializeMenu(){activateFunction=Ref(find(L"/Game/_Dawnwalker/UI/_Unified/GameHub/WBP_Window_GameHub.WBP_Window_GameHub_C:BP_OnActivated"));}
bool menuPending(){return injection.pending||wantsOpen;}
void menuHub(UObject* h){
    if(runtime.hub.matches(h)&&navbar.matches(object(h,L"NavBar"))&&tabButton){
        auto p=property(tabButton.get(),L"ButtonTag");if(p&&p->IsA<FStructProperty>()){auto type=static_cast<FStructProperty*>(p)->GetStruct();if(text(property(type,L"TagName"),p->ContainerPtrToValuePtr<void>(tabButton.get()))==tagName)return;}
    }
    bool open=wantsOpen;stopPreview();resetMenu();wantsOpen=open;runtime.hub=Ref(h);activateFunction=Ref(function(h,L"BP_OnActivated"));injection.request(runtime.now);
}
void menuRedraw(){redraw=true;}
void menuScriptPre(UObject* owner,UFunction* fn,void* params,Hook::TCallbackIterationData<void>& info){
    if(!runtime.hub.matches(owner)||!showFunction.matches(fn))return;
    if(ourTag(fn,params)){info.PreventOriginalFunctionCall();if(runtime.settings.enabled){if(runtime.playerReady)wantsOpen=true;else requestOpen();}}
    else if(runtime.menuOpen){runtime.menuOpen=false;stopPreview();writeStore();}
}
void menuScriptPost(UObject* owner,UFunction* fn,void*){
    // BP_OnActivated is delivered even when CommonUI calls its native
    // activation directly, bypassing the reflected ActivateWidget wrapper.
    // After binding, the normal miss path compares only function pointers.
    if(fn&&(activateFunction.address==fn||!activateFunction.address)){
        static const FName activated(L"BP_OnActivated"),hubClass(L"WBP_Window_GameHub_C");
        if(fn->GetFName()==activated&&owner&&owner->GetClassPrivate()->GetFName()==hubClass){activateFunction=Ref(fn);queueHub(owner);}
    }
    if(runtime.hub.matches(owner)&&deactivateFunction.matches(fn)){runtime.menuOpen=false;stopPreview();writeStore();}
    if(rebuildFunction.matches(fn)&&navbar.matches(owner)){tabButton={};injection.cancel();injection.request(runtime.now);}
}
void openMenu(){
    if(!runtime.player)return;wantsOpen=true;
    // The normal hub owns pause/input and navigation. Its creation event then
    // injects the Wardrobe button and selects this page.
    auto frontend=static_cast<UObject*>(nullptr);
    Call sub(find(L"/Script/Engine.Default__SubsystemBlueprintLibrary"),L"GetGameInstanceSubsystem");
    if(sub){sub.obj(L"ContextObject",runtime.player.get()).obj(L"Class",find(L"/Script/DogwoodUI.UIManagerSubsystem")).invoke();frontend=object(sub.resultObject(),L"ActiveFrontendWidget");}
    auto layer=object(frontend,L"GameMenuLayer");auto cls=asset(L"/Game/_Dawnwalker/UI/_Unified/GameHub/WBP_Window_GameHub.WBP_Window_GameHub_C");
    if(layer&&cls){
        Call current(layer,L"GetActiveWidget");current.invoke();auto h=current.resultObject();
        if(runtime.hub.matches(h)&&tabButton){show();return;}
        Call c(layer,L"BP_AddWidget");c.obj(L"ActivatableWidgetClass",cls).invoke();auto created=c.resultObject();if(!created)throw std::runtime_error("The game menu layer did not create a hub");menuHub(created);
    }
    else {wantsOpen=false;warn(L"Open the character menu once to initialize the Wardrobe tab.");}
}
void closeMenu(){
    wantsOpen=false;popup=0;if(!runtime.menuOpen){stopPreview();return;}runtime.menuOpen=false;stopPreview();
    if(auto hub=runtime.hub.get())call(hub,L"DeactivateWidget");writeStore();
}
void resetMenu(){
    auto oldPage=page;
    runtime.menuOpen=false;wantsOpen=false;popup=0;injection.cancel();page={};tree={};root={};grid={};title={};footer={};navbar={};tabButton={};previewImage={};showFunction={};deactivateFunction={};rebuildFunction={};activateFunction={};buttons.clear();controls.clear();pendingControls.clear();filtered.clear();pageChoices.clear();built=false;
    if(auto p=oldPage.get())try{call(p,L"RemoveFromParent");}catch(const std::exception& e){failure(L"Removing Wardrobe page",e);}
}
void stepMenu(){
    if(injection.ready(runtime.now)){auto ok=inject();injection.finish(ok,runtime.now);if(!ok&&!injection.pending){wantsOpen=false;warn(L"Wardrobe tab could not be added to this hub layout.");}return;}
    if(wantsOpen&&!injection.pending){if(!tabButton)throw std::runtime_error("Wardrobe tab was removed before opening");show();}
    if(!runtime.menuOpen)return;if(!page||!runtime.hub){runtime.menuOpen=false;stopPreview();resetMenu();return;}
    // Keep the same render doll for the entire visit. Appearance changes are
    // applied in place; saving, collection learning and filtering do not spawn it.
    if(redraw)draw();
    if(!pendingControls.empty()){for(unsigned i=0;i<2&&!pendingControls.empty();++i){auto control=std::move(pendingControls.front());pendingControls.pop_front();button(control.parent.get(),control.caption,std::move(control.click),true);}}
    else addTiles(); // One allocation budget shared by controls and choices.
    DWORD foreground{};GetWindowThreadProcessId(GetForegroundWindow(),&foreground);if(foreground!=GetCurrentProcessId())return;
    XINPUT_STATE state{};WORD pad{};BYTE left{},right{};if(XInputGetState(0,&state)==ERROR_SUCCESS){pad=state.Gamepad.wButtons;left=state.Gamepad.bLeftTrigger;right=state.Gamepad.bRightTrigger;}
    WORD edges=pad&~previousPad;
    bool escape=pressed(VK_ESCAPE)||(edges&XINPUT_GAMEPAD_B);if(escape){if(popup){popup=0;redraw=true;}else closeMenu();previousPad=pad;return;}
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
    if(pressed(VK_LEFT)||(edges&XINPUT_GAMEPAD_DPAD_LEFT))move(-1);
    if(pressed(VK_RIGHT)||(edges&XINPUT_GAMEPAD_DPAD_RIGHT))move(1);
    if(pressed(VK_UP)||(edges&XINPUT_GAMEPAD_DPAD_UP))move(-4);
    if(pressed(VK_DOWN)||(edges&XINPUT_GAMEPAD_DPAD_DOWN))move(4);
    bool click=pressed(VK_LBUTTON);POINT mouse{};GetCursorPos(&mouse);
    if(click){for(auto& control:controls)if(auto b=control.object.get()){Call hovered(b,L"IsHovered");hovered.invoke();if(hovered.resultInteger()){control.click();click=false;break;}}}
    if(click||mouse.x!=previousMouse.x||mouse.y!=previousMouse.y){for(unsigned i=0;i<buttons.size();++i)if(auto b=buttons[i].object.get()){Call hovered(b,L"IsHovered");hovered.invoke();if(hovered.resultInteger()){focus=i;if(click)buttons[i].click();break;}}previousMouse=mouse;}
    if((pressed(VK_RETURN)||(edges&XINPUT_GAMEPAD_A))&&focus<buttons.size())buttons[focus].click();
    previousPad=pad;previousLeft=left;previousRight=right;
}
}
