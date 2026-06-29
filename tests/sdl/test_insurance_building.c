#define SDL_MAIN_HANDLED
#include <SDL/SDL.h>
#undef main
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "app/property_flow.h"
#include "app/special_presentation_flow.h"
#include "core/game_event_payload.h"
#include "data/resource_catalog.h"
#include "platform/sdl1/game_event_presentation.h"
#include "platform/sdl1/game_loop_confirm_company_issue.h"
#include "platform/sdl1/game_loop_mouse_button_down_input.h"
#include "platform/sdl1/game_loop_mouse_button_up_input.h"
#include "platform/sdl1/game_flow_api.h"
#include "platform/sdl1/game_asset_api.h"
#include "platform/sdl1/render/blit.h"
#include "platform/sdl1/text/runtime_text.h"
#include "resource/mkf.h"

static GameSession session;
static GamePreviewAssets assets;

static void read_resource(Rich4Mkf *mkf, int index, unsigned char **bytes, size_t *size,
                          Rich4SprSmpInfo *spr)
{
    assert(rich4_mkf_read_resource(mkf, index, bytes, size, NULL) == RICH4_MKF_OK);
    if (spr) assert(rich4_spr_smp_probe(*bytes, *size, spr));
}

static void control_point(int control, int *x, int *y)
{
    size_t i;
    for (i = 0; i < 128u * 192u; i++) if (assets.ui.panel_data[22][i] == control) {
        *x = session.presentation.company_issue.entry_x + (int)(i % 128);
        *y = session.presentation.company_issue.entry_y + (int)(i / 128);
        return;
    }
    assert(0);
}

static void press_control(int control)
{
    int x, y;
    control_point(control, &x, &y);
    assert(game_company_issue_pointer(&session, &assets, NULL, 0, SDL_BUTTON_LEFT, x, y));
    assert(game_company_issue_pointer(&session, &assets, NULL, 2, SDL_BUTTON_LEFT, x, y));
}

int main(void)
{
    Rich4Mkf *data = NULL, *panel = NULL;
    ResourceCatalogEntry entry;
    SDL_Surface *screen, *reference;
    Rich4GameEventPresentationSnapshot snapshot = {&session.state, &session.presentation};
    GamePropertyPresentationState *p = &session.presentation.property;
    int i, x, y;
    assert(SDL_Init(SDL_INIT_TIMER) == 0);
    rich4_sdl_text_set_font_path("assets/fonts/rich4.ttc");
    screen = SDL_CreateRGBSurface(SDL_SWSURFACE, 640,480,16,0xf800,0x7e0,0x1f,0);
    reference = SDL_CreateRGBSurface(SDL_SWSURFACE, 640,480,16,0xf800,0x7e0,0x1f,0);
    assert(screen && reference);
    if (rich4_mkf_open("assets/original/Data.mkf", &data) != RICH4_MKF_OK)
        assert(rich4_mkf_open("assets/original/Data.mkf", &data) == RICH4_MKF_OK);
    if (rich4_mkf_open("assets/original/Panel.mkf", &panel) != RICH4_MKF_OK)
        assert(rich4_mkf_open("assets/original/Panel.mkf", &panel) == RICH4_MKF_OK);
    read_resource(data,517,&assets.ui.common_dialog_data,&assets.ui.common_dialog_size,&assets.ui.common_dialog_spr);
    assets.ui.has_common_dialog = 1;
    assert(rich4_resource_catalog_data_confirm(&entry));
    read_resource(data,entry.resource_index,&assets.ui.confirm_dialog_data,&assets.ui.confirm_dialog_size,&assets.ui.confirm_dialog_spr);
    assets.ui.has_confirm_dialog = 1;
    read_resource(data,518,&assets.ui.player_choice_data,&assets.ui.player_choice_size,&assets.ui.player_choice_spr);
    assets.ui.has_player_choice = 1;
    read_resource(data,2,&assets.character.character_data,&assets.character.character_size,&assets.character.character_spr);
    assets.character.has_character_ui = 1;
    for (i=18;i<=19;i++) {
        read_resource(data,571+i,&assets.ui.card_effect_data[i],&assets.ui.card_effect_size[i],NULL);
        assets.ui.has_card_effect[i]=1;
    }
    read_resource(panel,21,&assets.ui.panel_data[21],&assets.ui.panel_size[21],&assets.ui.panel_spr[21]);
    read_resource(panel,22,&assets.ui.panel_data[22],&assets.ui.panel_size[22],NULL);
    assets.ui.has_panel[21]=assets.ui.has_panel_raw[22]=1;
    session.state.active=1; session.state.player_count=3; session.state.price_index=1;
    for(i=0;i<3;i++) { session.state.players[i].play_state=1; session.state.players[i].character=i; }
    p->insurance_active=1; p->actor=0; p->owner=1; p->amount=10000;
    p->phase=GAME_PROPERTY_PRESENT_INSURANCE_CARD; p->insurance_card=19;
    SDL_FillRect(screen,NULL,0); SDL_FillRect(reference,NULL,0);
    draw_insurance_charge(screen,&assets,&snapshot);
    draw_original_card_presentation(reference,&assets,19,"\xa8\xcf\xa5\xce\xa7\x4b\xb6\x4f\xa5\x64");
    for(y=200;y<456;y++)
        assert(memcmp((char*)screen->pixels+y*screen->pitch+138*2,
            (char*)reference->pixels+y*reference->pitch+138*2,165*2)==0);
    assert(SDL_SaveBMP(screen,"build/insurance-free-card.bmp")==0);
    p->phase=GAME_PROPERTY_PRESENT_INSURANCE_FREE_CONFIRM;
    SDL_FillRect(screen,NULL,0); draw_insurance_charge(screen,&assets,&snapshot);
    {
        int left,top,width,height,nonzero=0;
        assert(game_confirm_dialog_bounds(&assets,GAME_COMMAND_PURCHASE_PROPERTY,&left,&top,&width,&height));
        assert(left+width/2==220 && top+height/2==320);
        for(y=top;y<top+height;y++) for(x=left;x<left+width;x++)
            if (((Uint16*)((char*)screen->pixels+y*screen->pitch))[x]) nonzero++;
        assert(nonzero>1000);
        assert(game_handle_insurance_charge_input(&session,&assets,SDL_BUTTON_LEFT,
            left+width/4,top+height/2,100));
        assert(p->phase==GAME_PROPERTY_PRESENT_INSURANCE_CARD);
    }
    assert(SDL_SaveBMP(screen,"build/insurance-confirm.bmp")==0);
    p->phase=GAME_PROPERTY_PRESENT_INSURANCE_TARGET; p->insurance_card=18;
    {
        Rich4SprSmpChunk frame;
        assert(rich4_spr_smp_get_chunk(assets.ui.player_choice_data,assets.ui.player_choice_size,
            &assets.ui.player_choice_spr,0,&frame)==RICH4_SPR_SMP_OK);
        x=220-frame.x+12; y=320-frame.y+12;
        assert(game_insurance_target_at(&assets,&session.state,0,x,y)==1);
        assert(game_insurance_target_at(&assets,&session.state,0,x+80,y)==2);
        assert(game_insurance_target_at(&assets,&session.state,0,x+160,y)==-1);
        assert(game_insurance_target_at(&assets,&session.state,0,x,y+72)==-1);
    }
    SDL_FillRect(screen,NULL,0); draw_insurance_charge(screen,&assets,&snapshot);
    assert(SDL_SaveBMP(screen,"build/insurance-target.bmp")==0);
    p->phase=GAME_PROPERTY_PRESENT_INSURANCE_SUBSTITUTE_NOTICE; p->insurance_payer=2;
    SDL_FillRect(screen,NULL,0); draw_insurance_charge(screen,&assets,&snapshot);
    assert(SDL_SaveBMP(screen,"build/insurance-substitute.bmp")==0);
    p->insurance_active=0;
    session.presentation.company_issue.active=1;
    session.presentation.company_issue.maximum=500;
    assert(rich4_company_issue_confirm(&session,1,NULL));
    press_control(2); assert(session.presentation.company_issue.quantity==500);
    press_control(4); assert(session.presentation.company_issue.quantity==0);
    press_control(7); assert(session.presentation.company_issue.quantity==7);
    press_control(6); assert(session.presentation.company_issue.quantity==0);
    press_control(2);
    press_control(16); assert(session.presentation.company_issue.quantity<500);
    control_point(1,&x,&y);
    {
        GameMouseButtonDownInputContext down = {0};
        GameMouseButtonUpInputContext up = {0};
        VirtualCursor cursor = {0};
        Rich4SceneId scene = RICH4_SCENE_GAME;
        int mouse_down = 0;
        cursor.x=x; cursor.y=y;
        down.app_state=&scene; down.session=&session; down.assets=&assets;
        down.cursor=&cursor; down.mouse_left_down=&mouse_down; down.button=SDL_BUTTON_LEFT;
        assert(game_handle_mouse_button_down_input(&down));
        assert(mouse_down==1 && !cursor.confirm_pressed);
        assert(game_company_issue_pointer(&session,&assets,NULL,1,SDL_BUTTON_LEFT,-100,-100));
        assert(session.presentation.company_issue.entry_x==0 && session.presentation.company_issue.entry_y==0);
        assert(game_company_issue_pointer(&session,&assets,NULL,1,SDL_BUTTON_LEFT,1000,1000));
        assert(session.presentation.company_issue.entry_x==512 && session.presentation.company_issue.entry_y==288);
        assert(game_company_issue_pointer(&session,&assets,NULL,1,SDL_BUTTON_LEFT,x+30,y+20));
        cursor.x=x+30; cursor.y=y+20;
        up.app_state=scene; up.session=&session; up.assets=&assets;
        up.cursor=&cursor; up.mouse_left_down=&mouse_down; up.button=SDL_BUTTON_LEFT;
        assert(game_handle_mouse_button_up_input(&up));
        assert(mouse_down==0 && !cursor.confirm_pressed);
    }
    assert(session.presentation.company_issue.entry_x==286 && session.presentation.company_issue.entry_y==164);
    press_control(2); assert(session.presentation.company_issue.quantity==500);
    SDL_FillRect(screen,NULL,0); draw_company_issue(screen,&assets,&snapshot);
    {
        int nonzero=0;
        for(y=164;y<356;y++) for(x=286;x<414;x++)
            if (((Uint16*)((char*)screen->pixels+y*screen->pitch))[x]) nonzero++;
        assert(nonzero>5000); /* complete original keypad, not only its slider */
    }
    assert(SDL_SaveBMP(screen,"build/insurance-quantity.bmp")==0);
    for(i=18;i<=19;i++) free(assets.ui.card_effect_data[i]);
    free(assets.ui.panel_data[21]); free(assets.ui.panel_data[22]);
    free(assets.ui.common_dialog_data); free(assets.ui.confirm_dialog_data);
    free(assets.ui.player_choice_data); free(assets.character.character_data);
    rich4_mkf_close(data); rich4_mkf_close(panel);
    SDL_FreeSurface(reference); SDL_FreeSurface(screen);
    rich4_sdl_text_shutdown(); SDL_Quit();
    puts("insurance building: original card pixels, target hit regions, keypad and drag: ok");
    return 0;
}
