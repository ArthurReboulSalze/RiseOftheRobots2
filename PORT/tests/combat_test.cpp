#define SDL_MAIN_HANDLED
#include "combat.h"
#include <cstdio>

static bool check(bool ok, const char* message) {
    if (!ok) std::fprintf(stderr,"%s\n",message);
    return ok;
}

static MvsBank bank() {
    MvsMove idle;
    idle.flags = 8; idle.ground_mode = 1;
    idle.sequences = {{{0,0,false},{0,0,false},{-1,0,true}}};
    MvsBank m; m.moves.resize(96,idle);
    m.moves[0].transitions = {{IN_PUNCH,8},{IN_KICK,9}};
    for (int i : {8,9,80,81,88,50}) {
        m.moves[i].flags = 6;
        m.moves[i].transitions.clear();
        m.moves[i].action_type = 2;
        m.moves[i].sequences = {
            {{1,0,false},{1,0,false},{1,0,false},{-1,0,true}},
            {{2,0,false},{2,0,false},{2,0,false},{-1,0,true}},
            {{3,0,false},{3,0,false},{3,0,false},{-1,0,true}}};
    }
    return m;
}

static void quarter_circle(Fighter& f, int direction, bool finishing=false) {
    f.sample_inputs(IN_DOWN);
    f.sample_inputs(IN_DOWN | (direction>0 ? IN_RIGHT : IN_LEFT));
    f.sample_inputs(direction>0 ? IN_RIGHT : IN_LEFT);
    f.sample_inputs(IN_PUNCH);
    f.step(IN_PUNCH,nullptr,finishing);
}

int main() {
    MvsBank moves = bank();
    for (int force=0; force<3; ++force) {
        Fighter f; f.mvs=&moves;
        f.step(IN_PUNCH | (force==1 ? IN_MEDIUM : force==2 ? IN_HEAVY : 0));
        if (!check(f.attack_strength==30+30*force && f.speed_level==force && f.current_frame()==1+force,
                   "all three configured attack keys must select different native streams")) return 1;
    }
    moves.commands = {{{1,2,18,16},80}};
    for (int direction : {1,-1}) {
        Fighter f; f.mvs=&moves; f.facing=direction;
        quarter_circle(f,direction);
        if (!check(f.move_id==80,"command history must recognize a short quarter circle in either facing")) return 1;
        for (int n=0;n<30;++n) f.step(IN_PUNCH);
        if (!check(f.move_id==0,"holding a completed command must not repeat it")) return 1;
    }
    moves.commands = {{{1,2,254,2},80}};
    Fighter gap; gap.mvs=&moves;
    for (int input : {2,0,2,1}) gap.sample_inputs(input);
    gap.step(IN_PUNCH);
    if (!check(gap.move_id==80,"FE must consume exactly one release or arbitrary history entry")) return 1;

    moves.commands = {{{1,2,18,16},88}};
    Fighter super; super.mvs=&moves;
    quarter_circle(super,1);
    if (!check(super.move_id!=88,"an empty super meter must reject the super command")) return 1;
    super=Fighter{}; super.mvs=&moves; super.super_meter=24;
    quarter_circle(super,1);
    if (!check(super.move_id==88 && super.super_meter==0,"a full super must consume the meter")) return 1;
    moves.commands[0].target=90;
    Fighter stolen; stolen.mvs=&moves;
    quarter_circle(stolen,1);
    if (!check(stolen.move_id!=90,"unearned stolen powers must stay locked")) return 1;
    stolen=Fighter{}; stolen.mvs=&moves; stolen.stolen_moves=1;
    quarter_circle(stolen,1);
    if (!check(stolen.move_id==90 && stolen.stolen_moves==0,"a granted stolen power must be consumed once")) return 1;
    moves.commands[0].target=50;
    Fighter finish; finish.mvs=&moves;
    quarter_circle(finish,1);
    if (!check(finish.move_id!=50,"finishings must be disabled during an ordinary round")) return 1;
    finish=Fighter{}; finish.mvs=&moves;
    quarter_circle(finish,1,true);
    if (!check(finish.move_id==50,"a finishing command must unlock in the post-round window")) return 1;

    EffectScript script{{1000,7,-2,0},{3,4,1,3},{4,5,2,3},{-2,0,0,0}};
    ScriptPlayer player; player.start(&script,100,80,-1);
    if (!check(player.image==1000 && player.x==93 && player.y==78,"invisible delay must still apply displacement")) return 1;
    player.advance(); player.advance(); player.advance();
    if (!check(player.image==3 && player.x==80 && player.y==82,"negative loop must jump back by eight-byte records")) return 1;
    EffectScript ending{{-999,0,0,0}};
    player.start(&ending,0,0,1);
    if (!check(!player.active(),"the short -999 terminator must stop an effect")) return 1;
    EffectScript broken{{-1,0,0,0}};
    player.start(&broken,0,0,1);
    if (!check(!player.active(),"invalid effect loops must terminate safely")) return 1;

    moves.commands.clear();
    Cl2Bank boxes; boxes.frames.resize(4);
    for (auto& frame : boxes.frames) frame.body_boxes={{20,130,10,20,3,2}};
    for (int i=1;i<4;++i) boxes.frames[i].attack_boxes={{20,130,10,20,10}};
    CombatData data; data.reactions={11,12,13,14};
    EffectScript impact{{0,0,0,2},{1,0,0,2}};
    data.impacts={impact,impact};
    auto pair = [&]() {
        std::array<Fighter,2> f;
        for (auto& p : f) {p.mvs=&moves;p.cl2=&boxes;p.robot_id=0;p.cl2_ref_x=0;}
        f[0].x=220;f[1].x=420;f[1].facing=-1;
        return f;
    };
    for (int force : {0,2}) {
        auto f=pair(); Combat combat(&data,nullptr,nullptr);
        combat.tick(f[0],f[1],IN_PUNCH|(force?IN_HEAVY:0),0);
        const int expected = force ? 32 : 10;
        if (!check(combat.hits().size()==1 && f[1].health==120-expected && f[1].move_id==13,
                   "damage must include strength, body multiplier and the matching reaction region")) return 1;
        for (int i=0;i<20;++i) combat.tick(f[0],f[1],IN_PUNCH,0);
        if (!check(f[1].health==120-expected,"a held strike must not drain health on every active frame")) return 1;
    }
    auto f=pair(); f[1].health=1;
    Combat combat(&data,nullptr,nullptr);
    combat.tick(f[0],f[1],IN_PUNCH,0);
    if (!check(combat.phase()==RoundPhase::FinishWindow && combat.winner()==0 && f[1].move_id==75,
               "KO must open the original stagger state and disable the defeated player's input")) return 1;
    for (int i=0;i<201;++i) combat.tick(f[0],f[1],0,IN_PUNCH|IN_UP);
    if (!check(combat.phase()==RoundPhase::Ending && !f[1].airborne(),"an expired finishing window must play normal defeat")) return 1;
    for (int i=0;i<110;++i) combat.tick(f[0],f[1],0,0);
    if (!check(combat.phase()==RoundPhase::Result,"round completion must settle into a stable result")) return 1;

    // A genuine negative CL2 byte drives the victim's sequence, without
    // assuming that every character has a universal target+1 finishing pair.
    f=pair(); f[1].health=1; combat.reset();
    combat.tick(f[0],f[1],IN_PUNCH,0);
    moves.commands={{{1,2,18,16},50}};
    boxes.frames[1].attack_boxes[0].damage_or_type=-51;
    f[0].sample_inputs(0); f[0].sample_inputs(IN_DOWN); f[0].sample_inputs(IN_DOWN|IN_RIGHT);
    f[0].sample_inputs(IN_RIGHT); f[0].sample_inputs(IN_PUNCH);
    combat.tick(f[0],f[1],IN_PUNCH,0);
    if (!check(combat.phase()==RoundPhase::Finishing && f[0].move_id==50 && f[1].move_id==51,
               "a finishing must play the source attack and signed collision reaction")) return 1;

    for (int button=0;button<6;++button) {
        f=pair();f[1].health=1;combat.reset();combat.set_easy_finishings(true);
        combat.sample_attack_buttons(1,0);
        combat.tick(f[0],f[1],IN_PUNCH,0);
        combat.sample_attack_buttons(1,0);combat.tick(f[0],f[1],0,0);
        if (!check(combat.phase()==RoundPhase::FinishWindow,"holding the knockout button must not trigger an easy finishing")) return 1;
        combat.sample_attack_buttons(0,0);combat.tick(f[0],f[1],0,0);
        combat.sample_attack_buttons(1<<button,0);combat.tick(f[0],f[1],0,0);
        if (!check(combat.phase()==RoundPhase::Finishing && f[0].move_id==50,
                   "each configured attack button must execute the single available finishing on a fresh press")) return 1;
    }
    moves.moves[48]=moves.moves[50]; moves.commands.push_back({{32,4},48});
    for (int button : {0,1,3,4}) {
        f=pair();f[1].health=1;combat.reset();combat.set_easy_finishings(true);
        combat.tick(f[0],f[1],IN_PUNCH,0);
        combat.sample_attack_buttons(1<<button,0);combat.tick(f[0],f[1],0,0);
        if (!check(f[0].move_id==(button%3==0 ? 48:50),"easy variants must have the same light/medium order for punches and kicks")) return 1;
    }
    f=pair();f[1].health=1;combat.reset();combat.set_easy_finishings(false);
    combat.tick(f[0],f[1],IN_PUNCH,0);combat.sample_attack_buttons(1,0);combat.tick(f[0],f[1],0,0);
    if (!check(combat.phase()==RoundPhase::FinishWindow,"disabled finishing assist must preserve original commands")) return 1;
    return 0;
}
