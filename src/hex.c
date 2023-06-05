#include "angband.h"

/* Flag list */
/*
p_ptr-magic_num1
0: Flag bits of spelling spells
1: Flag bits of despelled spells
2: Revange damage
p_ptr->magic_num2
0: Number of spelling spells
1: Type of revenge
2: Turn count for revenge
*/

#define MAX_KEEP 4

bool stop_hex_spell_all(void)
{
    int i;

    for (i = 0; i < 32; i++)
    {
        if (hex_spelling(i)) do_spell(REALM_HEX, i, SPELL_STOP);
    }

    p_ptr->magic_num1[0] = 0;
    p_ptr->magic_num2[0] = 0;

    /* Print message */
    if (p_ptr->action == ACTION_SPELL) set_action(ACTION_NONE);

    /* Redraw status */
    p_ptr->update |= (PU_BONUS | PU_HP | PU_MANA | PU_SPELLS);
    p_ptr->redraw |= (PR_EXTRA | PR_HP | PR_MANA | PR_STATUS);

    return TRUE;
}


bool stop_hex_spell(void)
{
    int spell;
    char choice;
    char out_val[160];
    bool flag = FALSE;
    int y = 1;
    int x = 20;
    int sp[MAX_KEEP];

    if (!hex_spelling_any())
    {
        msg_print("You are casting no spell.");
        return FALSE;
    }

    /* Stop all spells */
    else if ((p_ptr->magic_num2[0] == 1) || (p_ptr->lev < 35))
    {
        return stop_hex_spell_all();
    }
    else
    {
        strnfmt(out_val, 78, "Which spell do you stop casting? (Spell %c-%c, 'l' to all, ESC)",
            I2A(0), I2A(p_ptr->magic_num2[0] - 1));

        screen_save();

        while (!flag)
        {
            int n = 0;
            Term_erase(x, y, 255);
            prt("       ", y, x + 5);
            for (spell = 0; spell < 32; spell++)
            {
                if (hex_spelling(spell))
                {
                    Term_erase(x, y + n + 1, 255);
                    put_str(format("%c)  %s", I2A(n), do_spell(REALM_HEX, spell, SPELL_NAME)), y + n + 1, x + 2);
                    sp[n++] = spell;
                }
            }

            if (!get_com(out_val, &choice, TRUE)) break;
            if (isupper(choice)) choice = tolower(choice);

            if (choice == 'l')    /* All */
            {
                screen_load();
                return stop_hex_spell_all();
            }
            if ((choice < I2A(0)) || (choice > I2A(p_ptr->magic_num2[0] - 1))) continue;
            flag = TRUE;
        }
    }

    screen_load();

    if (flag)
    {
        int n = sp[A2I(choice)];

        do_spell(REALM_HEX, n, SPELL_STOP);
        p_ptr->magic_num1[0] &= ~(1L << n);
        p_ptr->magic_num2[0]--;
    }

    /* Redraw status */
    p_ptr->update |= (PU_BONUS | PU_HP | PU_MANA | PU_SPELLS);
    p_ptr->redraw |= (PR_EXTRA | PR_HP | PR_MANA | PR_STATUS);

    return flag;
}


/* Upkeeping hex spells
   Called from dungeon.c */
void check_hex(void)
{
    magic_type *s_ptr = 0;
    int spell;
    s32b need_mana;
    u32b need_mana_frac;
    bool res = FALSE;

    /* Spells spelled by player */
    if (p_ptr->realm1 != REALM_HEX) return;
    if (!p_ptr->magic_num1[0] && !p_ptr->magic_num1[1]) return;

    if (p_ptr->magic_num1[1])
    {
        p_ptr->magic_num1[0] = p_ptr->magic_num1[1];
        p_ptr->magic_num1[1] = 0;
        res = TRUE;
    }

    /* Stop all spells when anti-magic ability is given */
    if (p_ptr->anti_magic)
    {
        stop_hex_spell_all();
        return;
    }

    need_mana = 0;
    for (spell = 0; spell < 32; spell++)
    {
        if (hex_spelling(spell))
        {
            s_ptr = &technic_info[REALM_HEX - MIN_TECHNIC][spell];
            need_mana += mod_need_mana(s_ptr->smana, spell, REALM_HEX);
        }
    }


    /* Calculates final mana cost */
    need_mana_frac = 0;
    s64b_div(&need_mana, &need_mana_frac, 0, 3); /* Divide by 3 */
    need_mana += (p_ptr->magic_num2[0] - 1);


    /* Not enough mana */
    if (s64b_cmp(p_ptr->csp, p_ptr->csp_frac, need_mana, need_mana_frac) < 0)
    {
        stop_hex_spell_all();
        return;
    }

    /* Enough mana */
    else
    {
        s64b_sub(&(p_ptr->csp), &(p_ptr->csp_frac), need_mana, need_mana_frac);

        p_ptr->redraw |= PR_MANA;
        if (res)
        {
            msg_print("You restart spelling.");
            p_ptr->action = ACTION_SPELL;

            /* Recalculate bonuses */
            p_ptr->update |= (PU_BONUS | PU_HP);

            /* Redraw map and status bar */
            p_ptr->redraw |= (PR_MAP | PR_STATUS | PR_STATE);

            /* Update monsters */
            p_ptr->update |= (PU_MONSTERS);

            /* Window stuff */
            p_ptr->window |= (PW_OVERHEAD | PW_DUNGEON);
        }
    }

    /* Gain experiences of spelling spells */
    for (spell = 0; spell < 32; spell++)
    {
        if (!hex_spelling(spell)) continue;
        /* copied from cmd5 spell exp logic */
        if (!dun_level) continue; /* no exp in town */

        s16b cur_exp = p_ptr->spell_exp[spell];
        int ratio = (17 + s_ptr->slevel) * 100 / (10 + dun_level);
        point_t max_tbl[4] = { {60, 1600}, {100, 1200}, {200, 900}, {300, 0} };
        int max_exp = interpolate(ratio, max_tbl, 4);
        s16b exp_gain = 0;
        if (cur_exp < max_exp)
        {
            point_t gain_tbl[9] = { /* 0->900->1200->1400->1600 */
                {0, 128}, {200, 64}, {400, 32}, {600, 16},
                {800, 8}, {1000, 4}, {1200, 2}, {1400, 1}, {1600, 1} };
            exp_gain = interpolate(cur_exp, gain_tbl, 9);
            if (coffee_break) exp_gain *= 5;
            exp_gain /= 3; /* Divide by 3 to match mana cost, above */
        }
        if (exp_gain && (cur_exp + exp_gain) > max_exp) exp_gain = MAX(0, max_exp - cur_exp);

        if (exp_gain)
        {
            int old_level = spell_exp_level(cur_exp);
            int new_level = old_level;
            int max = SPELL_EXP_MASTER;

            p_ptr->spell_exp[spell] += exp_gain;
            if (p_ptr->spell_exp[spell] > max)
                p_ptr->spell_exp[spell] = max;
            new_level = spell_exp_level(p_ptr->spell_exp[spell]);
            if (new_level > old_level)
            {
                cptr desc[5] = { "Unskilled", "a Beginner", "Skilled", "an Expert", "a Master" };
                msg_format("You are now <color:B>%s</color> in <color:R>%s</color>.",
                    desc[new_level],
                    do_spell(REALM_HEX, spell % 32, SPELL_NAME));
            }
            else if (p_ptr->wizard || easy_damage)
            {
                msg_format("You now have <color:B>%d</color> proficiency in <color:R>%s</color>.",
                    p_ptr->spell_exp[spell],
                    do_spell(REALM_HEX, spell % 32, SPELL_NAME));
            }
            else if (p_ptr->spell_exp[spell]/100 > cur_exp/100)
            {
                msg_format("<color:B>You are getting more proficient with <color:R>%s</color>.</color>",
                    do_spell(REALM_HEX, spell % 32, SPELL_NAME));
            }
        }
    }

    /* Do any effects of continual spells */
    for (spell = 0; spell < 32; spell++)
    {
        if (hex_spelling(spell))
        {
            do_spell(REALM_HEX, spell, SPELL_CONT);
        }
    }
}


bool hex_spell_fully(void)
{
    int k_max = 0;

    k_max = (p_ptr->lev / 15) + 1;

    /* Paranoia */
    k_max = MIN(k_max, MAX_KEEP);

    if (p_ptr->magic_num2[0] < k_max) return FALSE;

    return TRUE;
}

void revenge_spell(void)
{
    if (p_ptr->realm1 != REALM_HEX) return;
    if (p_ptr->magic_num2[2] <= 0) return;

    switch(p_ptr->magic_num2[1])
    {
    case 1: do_spell(REALM_HEX, HEX_PATIENCE, SPELL_CONT); break;
    case 2: do_spell(REALM_HEX, HEX_REVENGE, SPELL_CONT); break;
    }
}

void revenge_store(int dam)
{
    if (p_ptr->realm1 != REALM_HEX) return;
    if (p_ptr->magic_num2[2] <= 0) return;

    p_ptr->magic_num1[2] += dam;
}


bool teleport_barrier(int m_idx)
{
    monster_type *m_ptr = &m_list[m_idx];
    monster_race *r_ptr = &r_info[m_ptr->r_idx];

    if ((!hex_spelling(HEX_ANTI_TELE)) && ((!prace_is_(RACE_MON_MUMMY)) ||
        (mummy_get_toggle() != MUMMY_TOGGLE_ANTITELE))) return FALSE;
    if ((p_ptr->lev * 3 / 2) < randint1(r_ptr->level)) return FALSE;

    return TRUE;
}


bool magic_barrier(int m_idx)
{
    monster_type *m_ptr = &m_list[m_idx];
    return magic_barrier_aux(m_ptr);
}

bool magic_barrier_aux(mon_ptr m_ptr)
{
    monster_race *r_ptr = &r_info[m_ptr->r_idx];

    if (!hex_spelling(HEX_ANTI_MAGIC)) return FALSE;
    if ((p_ptr->lev * 3 / 2) < randint1(r_ptr->level)) return FALSE;

    return TRUE;
}


bool multiply_barrier(int m_idx)
{
    monster_type *m_ptr = &m_list[m_idx];
    monster_race *r_ptr = &r_info[m_ptr->r_idx];

    if (!hex_spelling(HEX_ANTI_MULTI)) return FALSE;
    if ((p_ptr->lev * 3 / 2) < randint1(r_ptr->level)) return FALSE;

    return TRUE;
}

void hex_stop_spelling_spell(int cmd, variant *res)
{
    switch (cmd)
    {
    case SPELL_NAME:
        var_set_string(res, "Stop Spelling");
        break;
    case SPELL_DESC:
        var_set_string(res, "End one of your hex chants immediately.");
        break;
    case SPELL_ENERGY:
        var_set_int(res, 0);
        break;
    case SPELL_FLAGS:
        var_set_int(res, PWR_AFRAID | PWR_CONFUSED);
        break;
    case SPELL_CAST:
        var_set_bool(res, stop_hex_spell());
        break;
    default:
        default_spell(cmd, res);
        break;
    }
}
