#define SDL_MAIN_HANDLED
#include <SDL/SDL.h>
#undef main
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "app/special_presentation_flow.h"
#include "core/special_event.h"
#include "core/game_event_payload.h"
#include "platform/sdl1/game_flow_api.h"
#include "platform/sdl1/game_asset_api.h"
#include "platform/sdl1/game_event_presentation.h"
#include "platform/sdl1/game_loop_confirm_company_issue.h"
#include "platform/sdl1/game_loop_mouse_button_down_input.h"
#include "platform/sdl1/game_loop_mouse_button_up_input.h"
#include "platform/sdl1/render/game_panel_render.h"
#include "platform/sdl1/text/runtime_text.h"
#include "resource/mkf.h"

static GameSession session;
static GamePreviewAssets assets;

static void resource(Rich4Mkf *mkf, int id, unsigned char **data, size_t *size,
    Rich4SprSmpInfo *spr)
{
    assert(rich4_mkf_read_resource(mkf,id,data,size,NULL)==RICH4_MKF_OK);
    if (spr) assert(rich4_spr_smp_probe(*data,*size,spr));
}

static void begin(int id)
{
    GameEvent page;
    int i;
    memset(&session,0,sizeof(session));
    session.state.active=1; session.state.player_count=3;
    session.state.special_event_family=GAME_SPECIAL_EVENT_FORTUNE;
    session.state.special_event_phase=GAME_SPECIAL_PHASE_PREPARED;
    session.state.special_event_actor=0;
    session.state.special_event_logical_id=session.state.special_event_resolved_id=id;
    session.state.special_event_prepared_value=id==5 ? 0 : 3;
    for (i=0;i<3;i++) {
        session.state.players[i].play_state=1;
        session.state.players[i].character=i;
        session.state.players[i].card_inventory[0]=i+1;
    }
    session.state.players[1].card_inventory[1]=4;
    page=rich4_game_event_special_event_presentation(0,477+id,66,GAME_SPECIAL_EVENT_FORTUNE);
    assert(rich4_special_event_begin_from_event(&session,&page,0));
}

int main(void)
{
    Rich4Mkf *data=NULL,*panel=NULL;
    SDL_Surface *screen,*reference;
    Rich4GameEventPresentationSnapshot snapshot={&session.state,&session.presentation};
    Rich4SceneId scene=RICH4_SCENE_GAME;
    VirtualCursor cursor={0};
    GameMouseButtonDownInputContext down={0};
    GameMouseButtonUpInputContext up={0};
    int mouse_down=0,x,y,i,nonzero=0;
    assert(SDL_Init(SDL_INIT_TIMER)==0);
    rich4_sdl_text_set_font_path("assets/fonts/rich4.ttc");
    screen=SDL_CreateRGBSurface(SDL_SWSURFACE,640,480,16,0xf800,0x7e0,0x1f,0);
    reference=SDL_CreateRGBSurface(SDL_SWSURFACE,640,480,16,0xf800,0x7e0,0x1f,0);
    assert(screen && reference);
    if (rich4_mkf_open("assets/original/Data.mkf",&data)!=RICH4_MKF_OK)
        assert(rich4_mkf_open("assets/original/Data.mkf",&data)==RICH4_MKF_OK);
    if (rich4_mkf_open("assets/original/Panel.mkf",&panel)!=RICH4_MKF_OK)
        assert(rich4_mkf_open("assets/original/Panel.mkf",&panel)==RICH4_MKF_OK);
    resource(panel,11,&assets.ui.panel_data[11],&assets.ui.panel_size[11],&assets.ui.panel_spr[11]);
    assets.ui.has_panel[11]=1;
    resource(data,440,&assets.ui.confirm_dialog_data,&assets.ui.confirm_dialog_size,&assets.ui.confirm_dialog_spr);
    resource(data,517,&assets.ui.common_dialog_data,&assets.ui.common_dialog_size,&assets.ui.common_dialog_spr);
    resource(data,518,&assets.ui.player_choice_data,&assets.ui.player_choice_size,&assets.ui.player_choice_spr);
    resource(data,2,&assets.character.character_data,&assets.character.character_size,&assets.character.character_spr);
    assets.ui.has_confirm_dialog=assets.ui.has_common_dialog=assets.ui.has_player_choice=1;
    assets.character.has_character_ui=1;
    for (i=18;i<=20;i+=2) {
        resource(data,571+i,&assets.ui.card_effect_data[i],&assets.ui.card_effect_size[i],NULL);
        assets.ui.has_card_effect[i]=1;
    }
    begin(5);
    assert(rich4_special_event_prepare_choices(&session,1600));
    assert(game_fortune_choice_at(&session,&assets,19,75)==0);
    assert(game_fortune_choice_at(&session,&assets,99,75)==1);
    assert(game_fortune_choice_at(&session,&assets,419,75)==-1);
    assert(game_fortune_choice_at(&session,&assets,19,243)==-1);
    assert(game_fortune_choice_at(&session,&assets,19,275)==-1);
    SDL_FillRect(screen,NULL,0); SDL_FillRect(reference,NULL,0);
    draw_special_event_presentation(screen,&assets,&snapshot);
    assert(draw_game_panel_chunk(reference,&assets,11,0,14,70,0));
    for (y=70;y<90;y++) assert(!memcmp((char*)screen->pixels+y*screen->pitch+14*2,
        (char*)reference->pixels+y*reference->pitch+14*2,412*2));
    for (y=75;y<243;y++) for (x=19;x<419;x++)
        if (((Uint16*)((char*)screen->pixels+y*screen->pitch))[x]) nonzero++;
    assert(nonzero>1000);
    for (y=270;y<450;y++) for (x=14;x<426;x++)
        assert(((Uint16*)((char*)screen->pixels+y*screen->pitch))[x]==0);
    assert(SDL_SaveBMP(screen,"build/fortune-card-choice.bmp")==0);
    down.app_state=&scene; down.session=&session; down.assets=&assets;
    down.cursor=&cursor; down.mouse_left_down=&mouse_down; down.button=SDL_BUTTON_LEFT;
    up.app_state=scene; up.session=&session; up.assets=&assets;
    up.cursor=&cursor; up.mouse_left_down=&mouse_down; up.button=SDL_BUTTON_LEFT;
    cursor.x=100; cursor.y=80;
    assert(game_handle_mouse_button_down_input(&down));
    assert(session.state.players[0].card_inventory[1]==0);
    cursor.x=500; cursor.y=400; /* 0x4413EC confirms the left-down selection. */
    assert(game_handle_mouse_button_up_input(&up));
    assert(session.state.players[0].card_inventory[1]==4 && mouse_down==0);
    assert(session.presentation.special_event.fortune_card_target_player==2);
    assert(game_handle_fortune_choice_input(&session,&assets,SDL_BUTTON_RIGHT,0,0,2000));
    assert(session.state.players[2].card_inventory[0]==3);
    assert(session.presentation.special_event.fortune_choices_done);

    begin(6); session.state.players[0].card_inventory[0]=19;
    session.state.players[2].days_in_hospital=3;
    assert(rich4_special_event_prepare_choices(&session,1600));
    assert(session.presentation.special_event.phase==RICH4_SPECIAL_PRESENT_PASSIVE_CARD);
    SDL_FillRect(screen,NULL,0); draw_special_event_presentation(screen,&assets,&snapshot);
    assert(SDL_SaveBMP(screen,"build/fortune-scapegoat-card.bmp")==0);
    assert(rich4_special_event_prepare_choices(&session,3100));
    {
        Rich4SprSmpChunk frame;
        assert(rich4_spr_smp_get_chunk(assets.ui.player_choice_data,assets.ui.player_choice_size,
            &assets.ui.player_choice_spr,0,&frame)==RICH4_SPR_SMP_OK);
        x=220-frame.x+12; y=320-frame.y+12;
        assert(game_fortune_choice_at(&session,&assets,x+80,y)==2);
        SDL_FillRect(screen,NULL,0); draw_special_event_presentation(screen,&assets,&snapshot);
        assert(SDL_SaveBMP(screen,"build/fortune-scapegoat-target.bmp")==0);
        cursor.x=x+80; cursor.y=y;
        assert(game_handle_mouse_button_up_input(&up));
        assert(session.presentation.special_event.fortune_redirect_target==2);
    }
    begin(6); session.state.player_count=2; session.state.players[0].card_inventory[0]=19;
    assert(rich4_special_event_prepare_choices(&session,1600));
    assert(rich4_special_event_prepare_choices(&session,3100));
    {
        int left,top,width,height;
        assert(game_confirm_dialog_bounds(&assets,GAME_COMMAND_PURCHASE_PROPERTY,
            &left,&top,&width,&height));
        assert(game_handle_fortune_choice_input(&session,&assets,SDL_BUTTON_LEFT,
            left+width/4,top+height/2,3100));
        assert(session.presentation.special_event.fortune_redirect_target==1);
    }
    /* News 29's human is the selected operator, not the triggering actor. */
    begin(6);
    session.state.player_count = 2;
    session.state.special_event_family = GAME_SPECIAL_EVENT_NEWS;
    session.state.special_event_logical_id = session.state.special_event_resolved_id = 29;
    session.state.special_event_target_mask = (2u<<24) | (1u<<8) | 2u;
    session.state.players[1].card_inventory[0] = 19;
    {
        int left,top,width,height;
        GameEvent page = rich4_game_event_special_event_presentation(0,470,66,GAME_SPECIAL_EVENT_NEWS);
        assert(rich4_special_event_begin_from_event(&session,&page,0));
        assert(rich4_special_event_prepare_choices(&session,2400));
        assert(rich4_special_event_prepare_choices(&session,3900));
        assert(game_confirm_dialog_bounds(&assets,GAME_COMMAND_PURCHASE_PROPERTY,
            &left,&top,&width,&height));
        SDL_FillRect(screen,NULL,0);
        draw_special_event_presentation(screen,&assets,&snapshot);
        assert(game_handle_fortune_choice_input(&session,&assets,SDL_BUTTON_LEFT,
            left+width/4,top+height/2,3900));
        assert(session.presentation.special_event.fortune_redirect_target == 0);
    }
    /* Original god notice precedes AI card and target notices. */
    begin(6);
    session.state.players[0].play_state=2;
    session.state.players[0].god_type=8;
    session.state.players[0].god_land_modifier=-1;
    session.state.players[0].relation_balance[2]=10;
    session.state.players[0].card_inventory[0]=19;
    assert(rich4_special_event_prepare_choices(&session,1600));
    assert(session.presentation.special_event.phase==RICH4_SPECIAL_PRESENT_GOD_NOTICE);
    SDL_FillRect(screen,NULL,0); draw_special_event_presentation(screen,&assets,&snapshot);
    assert(SDL_SaveBMP(screen,"build/fortune-god-notice.bmp")==0);
    assert(rich4_special_event_prepare_choices(&session,3100));
    assert(session.presentation.special_event.phase==RICH4_SPECIAL_PRESENT_PASSIVE_CARD);
    assert(session.presentation.special_event.fortune_redirect_target==2);
    SDL_FillRect(screen,NULL,0); draw_special_event_presentation(screen,&assets,&snapshot);
    assert(SDL_SaveBMP(screen,"build/fortune-ai-scapegoat-card.bmp")==0);
    assert(rich4_special_event_prepare_choices(&session,4600));
    assert(session.presentation.special_event.phase==RICH4_SPECIAL_PRESENT_REDIRECT_NOTICE);
    SDL_FillRect(screen,NULL,0); draw_special_event_presentation(screen,&assets,&snapshot);
    assert(SDL_SaveBMP(screen,"build/fortune-ai-scapegoat-notice.bmp")==0);
    assert(session.state.players[0].card_inventory[0]==19);
    begin(6); session.state.players[0].play_state=2;
    session.state.players[0].card_inventory[0]=21;
    assert(rich4_special_event_prepare_choices(&session,1600));
    assert(session.presentation.special_event.phase==RICH4_SPECIAL_PRESENT_PASSIVE_CARD);
    SDL_FillRect(screen,NULL,0); draw_special_event_presentation(screen,&assets,&snapshot);
    assert(SDL_SaveBMP(screen,"build/fortune-ai-absolution-card.bmp")==0);
    for (i=18;i<=20;i+=2) free(assets.ui.card_effect_data[i]);
    free(assets.ui.panel_data[11]); free(assets.ui.confirm_dialog_data);
    free(assets.ui.common_dialog_data); free(assets.ui.player_choice_data);
    free(assets.character.character_data);
    rich4_mkf_close(data); rich4_mkf_close(panel);
    SDL_FreeSurface(screen); SDL_FreeSurface(reference);
    rich4_sdl_text_shutdown(); SDL_Quit();
    puts("fortune: original card-only panel, press/release, cancel and scapegoat targets: ok");
    return 0;
}
