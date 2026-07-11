// ─────────────────────────────────────────────────────────────────────────────
//  soccerlg SCC - 2026 Fausto Pracek (fpracek@gmail.com)
//  Segment 9 - Game State functions 1
// ─────────────────────────────────────────────────────────────────────────────

#include "msxgl.h"
#include "soccerlg.h"
#include "debug.h"
#include "input.h"
#include "soccerlg_rawdef.h"

void UpdateGameState(u8* game_state, u8* wait_secs, u8* start_sec, u16 target_ly) 
{
	if (*game_state < 3) {
		CallFnc_VOID_3PTR_U16(SEG_GAMESTATE_4, UpdateGameState_Init, game_state, wait_secs, start_sec, target_ly);
		// Dischetti visibili fin dallo scrolling di presentazione
		SwSprite[37].lx = PENALTY_DISH_X; SwSprite[37].frame = SPR_BIG_PENALTY_DISH;
		SwSprite[37].ly = (SwSprite[14].lx == PENALTY_DISH_X && SwSprite[14].ly == PENALTY_SOUTH_Y) ? 1000 : PENALTY_SOUTH_Y;
		SwSprite[38].lx = PENALTY_DISH_X; SwSprite[38].frame = SPR_BIG_PENALTY_DISH;
		SwSprite[38].ly = (SwSprite[14].lx == PENALTY_DISH_X && SwSprite[14].ly == PENALTY_NORTH_Y) ? 1000 : PENALTY_NORTH_Y;
	} else if (*game_state == 3) {
		// Gestione cambio tempo
		if (Mins == 0 && Secs == 0) {
			*wait_secs = 2;
			*start_sec = Frms;
			if (Half == 1) {
				*game_state = 4;
				CallFnc_VOID_16_P1(SEG_DRAW, ShowSpriteMessage, SPR_MSG_HALFTIME);
				CallFnc_VOID(SEG_EVENTS, EventHalfTime);
			} else if (Half == 2) {
				*game_state = 5;
				CallFnc_VOID_16_P1(SEG_DRAW, ShowSpriteMessage, SPR_MSG_TIMEUP);
				CallFnc_VOID(SEG_EVENTS, EventTimeUp);
			}
			return;
		}

		// Ciclo infinito attivo, pronti per giocare
		if (*wait_secs > 0) {
			if (*start_sec < Frms) { // Frms wrapped from 1 to 60
				(*wait_secs)--;
				if (*wait_secs == 0) {
					CallFnc_VOID(SEG_DRAW, HideSpriteMessage);
					TimerEnabled = TRUE; // Avvia il cronometro alla sparizione della scritta
				}
			}
			*start_sec = Frms;
			// NOTA: L'input viene sincronizzato da UpdateAllInputs nel loop principale
			return; // Ferma l'IA e il gioco finché la scritta non sparisce
		}

		// --- TELECAMERA E LIMITI ---
		CallFnc_VOID(SEG_FIELD, UpdateFieldCamera);
		CallFnc_VOID_3PTR(SEG_FIELD, CheckFieldBoundaries, game_state, wait_secs, start_sec);
		// Se un goal (o altro fallo) è appena stato rilevato, non proseguire: altrimenti
		// UpdateGameState_GlobalChecks rivaluterebbe presa portiere/fuorigioco sulla palla
		// ancora ferma in porta, sovrascrivendo lo stato 9 e impedendo il reset della palla
		// (loop infinito KICKOFF <-> IN GOAL sulle azioni rapide da rimessa).
		if (*game_state != 3) return;

		// --- AGGIORNAMENTO FRECCE ORIZZONTALI ---
		g_h_arrow_x += g_h_arrow_dir;
		if (g_h_arrow_x < 80) { g_h_arrow_x = 80; g_h_arrow_dir = 1; }
		else if (g_h_arrow_x > 162) { g_h_arrow_x = 162; g_h_arrow_dir = -1; }
		
		// Freccia in alto (per Team 1 che attacca verso il basso)
		// Visibile se P2 è umano (P1 vs P2)
		if (GameMode == GAMEMODE_P1_VS_P2) {
			SwSprite[24].lx = (u8)g_h_arrow_x;
			SwSprite[24].ly = 440; 
			SwSprite[24].frame = SPR_BIG_ARROW_BOTTOM;
		} else {
			SwSprite[24].ly = 1000; // Nascondimento assoluto
		}
		// Freccia in basso (per Team 2 che attacca verso l'alto)
		SwSprite[25].lx = (u8)g_h_arrow_x; SwSprite[25].ly = 50; SwSprite[25].frame = SPR_BIG_ARROW_TOP;

		struct ObjectInfo* Ball = &SwSprite[14];

		// Dischetti rigore visibili in entrambe le aree durante il gioco
		SwSprite[37].lx = PENALTY_DISH_X; SwSprite[37].frame = SPR_BIG_PENALTY_DISH;
		SwSprite[37].ly = (Ball->lx == PENALTY_DISH_X && Ball->ly == PENALTY_SOUTH_Y) ? 1000 : PENALTY_SOUTH_Y;
		SwSprite[38].lx = PENALTY_DISH_X; SwSprite[38].frame = SPR_BIG_PENALTY_DISH;
		SwSprite[38].ly = (Ball->lx == PENALTY_DISH_X && Ball->ly == PENALTY_NORTH_Y) ? 1000 : PENALTY_NORTH_Y;

		// --- AGGIORNAMENTO POSSESSO E FOCUS UMANO ---
		
		u8 closest_t1 = 1; u16 min_dist_t1 = 0xFFFF;
		u8 closest_t2 = 8; u16 min_dist_t2 = 0xFFFF;
		
		for (u8 i = 1; i < 7; i++) { 
			u8 dx_diff = (u8)(SwSprite[i].lx - Ball->lx);
			u16 dist_x = (dx_diff < 128) ? dx_diff : (256 - dx_diff);
			u16 dy_diff = (u16)(SwSprite[i].ly - Ball->ly) & 511;
			u16 dist_y = (dy_diff < 256) ? dy_diff : (512 - dy_diff);
			u16 dist = dist_x + dist_y;
			if (LastTouchTeam == TEAM_1 && i == LastTouchPlayer && Ball->anim < 5 && g_is_ball_carried) dist = 0; // Forza il focus sul portatore (solo se ha davvero palla ai piedi)
			if (dist < min_dist_t1) { min_dist_t1 = dist; closest_t1 = i; }
		}
		for (u8 i = 8; i < 14; i++) {
			u8 dx_diff = (u8)(SwSprite[i].lx - Ball->lx);
			u16 dist_x = (dx_diff < 128) ? dx_diff : (256 - dx_diff);
			u16 dy_diff = (u16)(SwSprite[i].ly - Ball->ly) & 511;
			u16 dist_y = (dy_diff < 256) ? dy_diff : (512 - dy_diff);
			u16 dist = dist_x + dist_y;
			if (LastTouchTeam == TEAM_2 && i == LastTouchPlayer && Ball->anim < 5 && g_is_ball_carried) dist = 0; // Forza il focus sul portatore (solo se ha davvero palla ai piedi)
			if (dist < min_dist_t2) { min_dist_t2 = dist; closest_t2 = i; }
		}

		g_closest_t1 = closest_t1;
		g_closest_t2 = closest_t2;

		// Memorizza se il trigger è stato usato per il cambio giocatore
		bool t1_switched = FALSE;
		bool t2_switched = FALSE;

		// Assegna il cursore di controllo al giocatore più vicino
		if (LastTouchTeam == TEAM_2 || LastTouchTeam == 0xFF || T2_Carrier == 0xFF) {
			T2_Carrier = closest_t2;
		} else if (g_player_input[1].trigger_pressed) {
			u16 b_dist_x = (SwSprite[T2_Carrier].lx > Ball->lx) ? (SwSprite[T2_Carrier].lx - Ball->lx) : (Ball->lx - SwSprite[T2_Carrier].lx);
			u16 b_dist_y = (SwSprite[T2_Carrier].ly > Ball->ly) ? (SwSprite[T2_Carrier].ly - Ball->ly) : (Ball->ly - SwSprite[T2_Carrier].ly);
			if ((b_dist_x > 48 || b_dist_y > 24) && closest_t2 != T2_Carrier) {
				T2_Carrier = closest_t2; // Cambio manuale del giocatore in difesa!
				t2_switched = TRUE;
			}
		}
		
		if (GameMode == GAMEMODE_P1_VS_P2) {
			if (LastTouchTeam == TEAM_1 || LastTouchTeam == 0xFF || T1_Carrier == 0xFF) {
				T1_Carrier = closest_t1;
			} else if (g_player_input[0].trigger_pressed) {
				u16 b_dist_x = (SwSprite[T1_Carrier].lx > Ball->lx) ? (SwSprite[T1_Carrier].lx - Ball->lx) : (Ball->lx - SwSprite[T1_Carrier].lx);
				u16 b_dist_y = (SwSprite[T1_Carrier].ly > Ball->ly) ? (SwSprite[T1_Carrier].ly - Ball->ly) : (Ball->ly - SwSprite[T1_Carrier].ly);
				if ((b_dist_x > 48 || b_dist_y > 24) && closest_t1 != T1_Carrier) {
					T1_Carrier = closest_t1; // Cambio manuale del giocatore in difesa!
					t1_switched = TRUE;
				}
			}
		} else {
			T1_Carrier = 0xFF;
		}

		// Aggiorna il bersaglio del passaggio in base alla direzione dello sguardo
		// Mostra il bersaglio SOLO se la propria squadra ha il possesso della palla (o è palla contesa iniziale)
if (min_dist_t2 <= 24 && (LastTouchTeam == TEAM_2 || LastTouchTeam == 0xFF)) {
					if (T2_Receiver == 0xFF || (Frms % 8) == 0) // Throttle: evita il cambio repentino di destinatario
						T2_Receiver = (u8)CallFnc_U16_P4B(SEG_HELPERS, FindReceiver, T2_Carrier, 0xFF, g_last_dx[1], g_last_dy[1]);
				} else T2_Receiver = 0xFF;
		
		if (GameMode == GAMEMODE_P1_VS_P2) {
					if (min_dist_t1 <= 24 && (LastTouchTeam == TEAM_1 || LastTouchTeam == 0xFF)) {
						if (T1_Receiver == 0xFF || (Frms % 8) == 0) // Throttle: evita il cambio repentino di destinatario
							T1_Receiver = (u8)CallFnc_U16_P4B(SEG_HELPERS, FindReceiver, T1_Carrier, 0xFF, g_last_dx[0], g_last_dy[0]);
					} else T1_Receiver = 0xFF;
		}

		// --- ANIMAZIONE DRIBBLING PALLA E PORTATORE ---

		CallFnc_VOID_3PTR(SEG_HELPERS, UpdateGameState_GlobalChecks, game_state, wait_secs, start_sec);
		if (*game_state == 6 || (g_is_penalty_shootout && RestartType == RESTART_GKSAVE)) return;
		
		// 1. Fisica della palla
		CallFnc_VOID(SEG_GAMESTATE_8, UpdateBallPhysics);

		// --- CONTROLLO OFFSIDE GLOBALE (CPU e UMANI) ---
		if (g_pass_receiver & 0x80) {
			u8 rec = g_pass_receiver & 0x7F;
			if (rec >= 14) {
				g_pass_receiver = 0xFF;
			} else {
				u8 pass_team = (rec < 7) ? TEAM_1 : TEAM_2;

				if (LastTouchTeam != 0xFF && LastTouchTeam != pass_team) {
					g_pass_receiver = 0xFF; // Intercettata dagli avversari: annulla offside
				} else if (Ball->anim == 5) {
					// Palla in volo verso ricevitore in fuorigioco: fischia subito
					*game_state = 6;
					RestartType = RESTART_OFFSIDE;
					RestartSideX = (u8)g_pass_target_x;
					RestartSideY = g_pass_target_y;
					CallFnc_VOID(SEG_EVENTS, EventOffside);
					Ball->anim = Ball->dx = Ball->dy = 0;
					Ball->lx = g_pass_target_x;
					Ball->ly = g_pass_target_y;
					Ball->frame = SPR_BALL_SIZE_1;
					T1_Carrier = T2_Carrier = 0xFF;
					g_pass_receiver = 0xFF;
					TimerEnabled = FALSE;
					*wait_secs = 2; *start_sec = Frms;
					return;
				}
			}
		}

		g_is_ball_carried = FALSE;
		if (LastTouchPlayer != 0xFF && Ball->anim < 5) {
			u16 c_dist_x = (SwSprite[LastTouchPlayer].lx > Ball->lx) ? (SwSprite[LastTouchPlayer].lx - Ball->lx) : (Ball->lx - SwSprite[LastTouchPlayer].lx);
			u16 c_dist_y = (SwSprite[LastTouchPlayer].ly > Ball->ly) ? (SwSprite[LastTouchPlayer].ly - Ball->ly) : (Ball->ly - SwSprite[LastTouchPlayer].ly);
			if (c_dist_x <= 24 && c_dist_y <= 24) g_is_ball_carried = TRUE;
		}

		// 2. Gestione portatori (Player 1 e Player 2)
		u8 carriers[2] = {T1_Carrier, T2_Carrier};
		u8 receivers[2] = {T1_Receiver, T2_Receiver};

		for (u8 i = 0; i < 2; i++) { // i=0 per Team 1 (P2/CPU), i=1 per Team 2 (P1)
			u8 carrier = carriers[i];
			if (carrier == 0xFF) continue;
			
			u8 dir = g_player_input[i].direction;
			bool trigger_pressed = g_player_input[i].trigger_pressed;
			
			// Disabilita il trigger per il resto del loop se l'abbiamo appena usato per cambiare giocatore
			if (i == 0 && t1_switched) trigger_pressed = FALSE;
			if (i == 1 && t2_switched) trigger_pressed = FALSE;
			
			struct ObjectInfo* Carrier = &SwSprite[carrier];
			u8 carrier_team = (carrier < 7) ? TEAM_1 : TEAM_2;

			// --- GESTIONE SCIVOLATA UMANA ---
			if (Carrier->count > 0) {
				Carrier->count--;

				if (Carrier->count >= 20) {
					Carrier->lx += Carrier->dx;
					Carrier->ly += Carrier->dy;
					
					if (Carrier->lx < 16) Carrier->lx = 16;
					if (Carrier->lx > 224) Carrier->lx = 224;
					if (Carrier->ly < 24) Carrier->ly = 24;
					if (Carrier->ly > 488) Carrier->ly = 488;

					Carrier->frame = (Carrier->dx > 0) ? 
								((carrier_team == TEAM_1) ? SPR_T1_PLAYER_TACKLE_FROM_WEST : SPR_T2_PLAYER_TACKLE_FROM_WEST) :
								((carrier_team == TEAM_1) ? SPR_T1_PLAYER_TACKLE_FROM_EAST : SPR_T2_PLAYER_TACKLE_FROM_EAST);

					// Controllo furto durante la scivolata
					u16 b_dist_x = (Carrier->lx > Ball->lx) ? (Carrier->lx - Ball->lx) : (Ball->lx - Carrier->lx);
					u16 b_dist_y = (Carrier->ly > Ball->ly) ? (Carrier->ly - Ball->ly) : (Ball->ly - Carrier->ly);
					
					bool can_steal = (b_dist_x <= 24 && b_dist_y <= 24);
					if (!can_steal && g_is_ball_carried && LastTouchPlayer != 0xFF && LastTouchTeam != carrier_team) {
						u16 c_dist_y = (Carrier->ly > SwSprite[LastTouchPlayer].ly) ? (Carrier->ly - SwSprite[LastTouchPlayer].ly) : (SwSprite[LastTouchPlayer].ly - Carrier->ly);
						if (b_dist_x <= 32 && c_dist_y <= 24) can_steal = TRUE;
					}

					bool is_immune_tackle = (Ball->count > 0 && LastTouchTeam != carrier_team && LastTouchTeam != 0xFF);

					if (can_steal && Ball->anim < 5 && RestartType == 0 && !is_immune_tackle) {
						if (LastTouchTeam != carrier_team) {
							Ball->count = 30; // Immunità aumentata
							g_pass_receiver = 0xFF; // Intercetto: disinnesca fuorigioco
						}
						LastTouchTeam = carrier_team;
						LastTouchPlayer = carrier;
						if (Ball->anim > 3) Ball->anim = 3;
						Ball->frame = SPR_BALL_SIZE_1;
						Carrier->count = 0; // Ferma la scivolata appena ruba palla
					}
				} else {
					if (Carrier->count >= 10) {
						Carrier->frame = (Carrier->dx > 0) ? 
									((carrier_team == TEAM_1) ? SPR_T1_PLAYER_TACKLE_FROM_WEST : SPR_T2_PLAYER_TACKLE_FROM_WEST) :
									((carrier_team == TEAM_1) ? SPR_T1_PLAYER_TACKLE_FROM_EAST : SPR_T2_PLAYER_TACKLE_FROM_EAST);
					} else {
						Carrier->frame = CallFnc_U16_P3(SEG_GAMESTATE_9, GetPlayerIdleFrame, carrier, 0, (carrier_team == TEAM_1) ? 1 : -1);
					}
				}
				continue; // Salta il resto dei comandi e dell'animazione
			}
			
			Carrier->dx = 0; Carrier->dy = 0;
			switch(dir) {
				case DIRECTION_UP: Carrier->dy = -2; break;
				case DIRECTION_UP_RIGHT: Carrier->dy = -2; Carrier->dx = 2; break;
				case DIRECTION_RIGHT: Carrier->dx = 2; break;
				case DIRECTION_DOWN_RIGHT: Carrier->dy = 2; Carrier->dx = 2; break;
				case DIRECTION_DOWN: Carrier->dy = 2; break;
				case DIRECTION_DOWN_LEFT: Carrier->dy = 2; Carrier->dx = -2; break;
				case DIRECTION_LEFT: Carrier->dx = -2; break;
				case DIRECTION_UP_LEFT: Carrier->dy = -2; Carrier->dx = -2; break;
			}
			
			// Se il giocatore si muove
			if (Carrier->dx != 0 || Carrier->dy != 0) {
				g_last_dx[i] = Carrier->dx;
				g_last_dy[i] = Carrier->dy;
				
				i16 next_x = (i16)Carrier->lx + Carrier->dx;
				if (next_x < 16) Carrier->lx = 16;
				else if (next_x > 224) Carrier->lx = 224;
				else Carrier->lx = (u8)next_x;
				
				i16 next_y = (i16)Carrier->ly + Carrier->dy;
				if (next_y < 24) Carrier->ly = 24;
				else if (next_y > 488) Carrier->ly = 488;
				else Carrier->ly = (u16)next_y;
				
				Carrier->anim++;
				const u8 walk_seq[4] = {0, 1, 2, 1}; 
				Carrier->frame = CallFnc_U16_P4(SEG_GAMESTATE_9, GetPlayerAnimFrame, carrier, Carrier->dx, Carrier->dy, walk_seq[(Carrier->anim / 3) % 4]);
			} else {
				// Se è fermo, usa l'ultima direzione di movimento per la posa di attesa
				Carrier->frame = CallFnc_U16_P3(SEG_GAMESTATE_9, GetPlayerIdleFrame, carrier, g_last_dx[i], g_last_dy[i]);
			}
			
			// Calcolo della distanza dalla palla (SEMPRE, sia che si muova che fermo)
			u16 dist_x = (Carrier->lx > Ball->lx) ? (Carrier->lx - Ball->lx) : (Ball->lx - Carrier->lx);
			u16 dist_y = (Carrier->ly > Ball->ly) ? (Carrier->ly - Ball->ly) : (Ball->ly - Carrier->ly);
			
			// Tolleranza allargata a 32 pixel se il giocatore è già il legittimo portatore per ammortizzare 
			// i bruschi cambi di direzione (es: da diagonale a dritto) o le partenze repentine.
			u8 touch_dist = (LastTouchTeam == carrier_team || LastTouchTeam == 0xFF) ? 32 : (g_is_ball_carried ? 10 : 14); 
			if (Ball->anim >= 6) touch_dist = 12; // I tiri potenti sfuggono facilmente al tackle
			
			bool is_immune = (Ball->count > 0 && LastTouchTeam != carrier_team && LastTouchTeam != 0xFF);

			// Per portatore in moto E/W puro la palla ha un offset Y visivo: controlla distanza dal portatore
			u16 eff_dist_y = dist_y;
			u8 touch_dist_y = touch_dist;
			if (g_is_ball_carried && LastTouchPlayer != 0xFF && LastTouchTeam != carrier_team && SwSprite[LastTouchPlayer].dy == 0) {
				touch_dist_y = 9;
				eff_dist_y = (Carrier->ly >= SwSprite[LastTouchPlayer].ly) ?
					(u16)(Carrier->ly - SwSprite[LastTouchPlayer].ly) :
					(u16)(SwSprite[LastTouchPlayer].ly - Carrier->ly);
			}
			// Il contatto con la palla portata attivamente dall'avversario richiede un'azione esplicita
			// (trigger per il giocatore umano, scivolata/tackle per la CPU via PlayerAI).
			// Senza questo blocco, basta passarci vicino per rubare palla al momento del cambio direzione.
			bool actively_carried_by_opp = (g_is_ball_carried && LastTouchTeam != carrier_team && LastTouchTeam != 0xFF);

			// Se il giocatore tocca fisicamente la palla (e non è in volo)
			if (dist_x <= touch_dist && eff_dist_y <= touch_dist_y && Ball->anim < 5 && !is_immune && RestartType == 0 && !actively_carried_by_opp) {
					if (LastTouchTeam != carrier_team) {
						Ball->count = 30; // Immunità aumentata
					} else if (LastTouchPlayer != carrier) {
						Ball->count = 20; // Immunità alla ricezione del passaggio
					}
					LastTouchTeam = (carrier < 7) ? TEAM_1 : TEAM_2;
					LastTouchPlayer = carrier;
					Ball->frame = SPR_BALL_SIZE_1; // Assicura che la palla sia a terra quando tra i piedi
					g_pass_receiver = 0xFF; // Resetta il ricevitore adesso che ha palla
					
					// Ricalcola il ricevitore adesso che il portatore ha la palla
					i8 c_dx, c_dy;
					if (Carrier->dx != 0 || Carrier->dy != 0) {
						c_dx = (Carrier->dx > 0) ? 1 : ((Carrier->dx < 0) ? -1 : 0);
						c_dy = (Carrier->dy > 0) ? 1 : ((Carrier->dy < 0) ? -1 : 0);
					} else {
						// Se fermo, usa l'ultima direzione conosciuta per orientare la palla
						c_dx = (g_last_dx[i] > 0) ? 1 : ((g_last_dx[i] < 0) ? -1 : 0);
						c_dy = (g_last_dy[i] > 0) ? 1 : ((g_last_dy[i] < 0) ? -1 : 0);
					}
				receivers[i] = (u8)CallFnc_U16_P4B(SEG_HELPERS, FindReceiver, carrier, 0xFF, c_dx, c_dy);
					
					// AZIONE DEL GIOCATORE: Passaggio o Dribbling
					bool action_taken = FALSE;
					if (trigger_pressed) {						
						bool is_shooting = FALSE;
						u8 dir = g_player_input[i].direction;

						// Condizioni per il tiro
						// Team 2 (P1, sud) attacca verso Nord
						if (i == 1 && Carrier->ly < 256 && Field.ly == 0) {
							if (dir == DIRECTION_UP || dir == DIRECTION_UP_LEFT || dir == DIRECTION_UP_RIGHT) {
								is_shooting = TRUE;
							}
						}
						// Team 1 (P2, nord) attacca verso Sud
						else if (i == 0 && GameMode == GAMEMODE_P1_VS_P2 && Carrier->ly > 256 && Field.ly == (FIELD_HEIGHT - 192)) {
							if (dir == DIRECTION_DOWN || dir == DIRECTION_DOWN_LEFT || dir == DIRECTION_DOWN_RIGHT) {
								is_shooting = TRUE;
							}
						}

						if (is_shooting) {
							action_taken = TRUE;
							Ball->anim = 0; Ball->count = 0;
							g_pass_receiver = 0xFF; 
							
							g_pass_start_x = Carrier->lx;
							g_pass_start_y = Carrier->ly;
							
							g_pass_target_x = g_h_arrow_x; // Direzione della freccia
							g_pass_target_y = (i == 1) ? 16 : 496; // Dentro la porta
							
							u16 r_dx = (g_pass_target_x > g_pass_start_x) ? (g_pass_target_x - g_pass_start_x) : (g_pass_start_x - g_pass_target_x);
							u16 r_dy = (g_pass_target_y > g_pass_start_y) ? (g_pass_target_y - g_pass_start_y) : (g_pass_start_y - g_pass_target_y);
							
							g_pass_max_frames = (r_dx + r_dy) / 8; // Tiro potente e veloce
							if (g_pass_max_frames < 10) g_pass_max_frames = 10;
							if (g_pass_max_frames > 25) g_pass_max_frames = 25;
							g_pass_max_height = 2; // Tiro rasoterra e limitato
							
							Ball->anim = 5;
							CallFnc_VOID(SEG_EVENTS, EventBallKicked);
							PlaySCCEvent(DANGER_KICK_BIN_SEG, DANGER_KICK_BIN_SIZE);
						} else {
							// Se non si tira, si passa
							u8 receiver = receivers[i];
						
							if (receiver != 0xFF) {
								// === CONTROLLO OFFSIDE AL MOMENTO DEL PASSAGGIO ===
								bool is_offside = FALSE;
								if (i == 1) { // Team 2 (P1)
									u16 offside_line = (SwSprite[1].ly < SwSprite[2].ly) ? SwSprite[1].ly : SwSprite[2].ly;
									if (Carrier->ly < offside_line) offside_line = Carrier->ly; // Regola linea palla
									if (SwSprite[receiver].ly < offside_line - 8 && SwSprite[receiver].ly < 256) is_offside = TRUE;
								} else { // Team 1 (CPU/P2)
									u16 offside_line = (SwSprite[8].ly > SwSprite[9].ly) ? SwSprite[8].ly : SwSprite[9].ly;
									if (Carrier->ly > offside_line) offside_line = Carrier->ly; // Regola linea palla
									if (SwSprite[receiver].ly > offside_line + 8 && SwSprite[receiver].ly > 256) is_offside = TRUE;
								}

								// === PASSAGGIO DIRETTO AL DESTINATARIO - FORZA PASSAGGIO ===
								Ball->anim = 0; Ball->count = 0;
								
								g_pass_receiver = receiver | (is_offside ? 0x80 : 0);
								g_pass_start_x = Carrier->lx;
								g_pass_start_y = Carrier->ly;
								g_pass_target_x = SwSprite[receiver].lx;
								g_pass_target_y = SwSprite[receiver].ly;
								
								u16 r_dx = (g_pass_target_x > g_pass_start_x) ? (g_pass_target_x - g_pass_start_x) : (g_pass_start_x - g_pass_target_x);
								u16 r_dy = (g_pass_target_y > g_pass_start_y) ? (g_pass_target_y - g_pass_start_y) : (g_pass_start_y - g_pass_target_y);
								
								g_pass_max_frames = (r_dx + r_dy) / 6; // Velocità aumentata
								if (g_pass_max_frames < 6) g_pass_max_frames = 6;
								if (g_pass_max_frames > 28) g_pass_max_frames = 28;
								g_pass_max_height = (r_dx + r_dy) / 24; // Altezza proporzionale alla distanza
								if (g_pass_max_height < 1) g_pass_max_height = 1;
								if (g_pass_max_height > 7) g_pass_max_height = 7;
								
								Ball->anim = 5; // Flag per passaggio
								CallFnc_VOID(SEG_EVENTS, EventBallKicked);
								action_taken = TRUE;
							}
						}
					}
					
					if (!action_taken && (Ball->dx != c_dx || Ball->dy != c_dy)) {
						// Cambio direzione fluido: la palla si riavvicina dolcemente ai piedi
						// invece di teletrasportarsi, evitando uscite dal campo accidentali.
						bool is_180_turn = (Ball->dx == -c_dx && Ball->dy == -c_dy && (c_dx != 0 || c_dy != 0));

						i8 off_x = 0; i8 off_y = 6;
						if (c_dx > 0) off_x = (c_dy > 0) ? 3 : 8; else if (c_dx < 0) off_x = (c_dy > 0) ? -3 : -8;
						if (c_dy > 0) off_y = (c_dx != 0) ? 11 : 8; else if (c_dy < 0) off_y = -2;
						
						Ball->dx = c_dx;
						Ball->dy = c_dy;
						
						i16 ideal_x = (i16)Carrier->lx + off_x;
						i16 ideal_y = (i16)Carrier->ly + off_y;
						Ball->lx = (u8)(((i16)Ball->lx + ideal_x) / 2);
						Ball->ly = (u16)(((i16)Ball->ly + ideal_y) / 2) & 511;
						
						if (Carrier->dx != 0 || Carrier->dy != 0) {
							Ball->anim = (c_dx != 0 && c_dy != 0) ? 1 : 2; // Tocco cortissimo in diagonale, normale in rettilineo
							Ball->count = is_180_turn ? 0 : 12; // Nessuna immunità se torna indietro di 180 gradi
							CallFnc_VOID(SEG_EVENTS, EventBallKicked);
						} else {
							Ball->anim = 0;
						}
					} else if (!action_taken) {
						// Stessa direzione: kick solo quando la palla è ferma (anim==0).
						// Quando anim>0, UpdateBallPhysics gestisce il rotolamento; non sovrascrivere.
						if (Ball->anim == 0) {
							i8 off_x = 0; i8 off_y = 6;
							if (c_dx > 0) off_x = (c_dy > 0) ? 3 : 8; else if (c_dx < 0) off_x = (c_dy > 0) ? -3 : -8;
							if (c_dy > 0) off_y = (c_dx != 0) ? 11 : 8; else if (c_dy < 0) off_y = -2;
							// Snap esatto alla posizione corretta, poi calcio
							Ball->lx = (u8)((i16)Carrier->lx + off_x);
							Ball->ly = (u16)((i16)Carrier->ly + off_y) & 511;
							Ball->dx = c_dx; Ball->dy = c_dy;
							if (Carrier->dx != 0 || Carrier->dy != 0) {
							Ball->anim = (c_dx != 0 && c_dy != 0) ? 2 : 4; // Tocco corto in diagonale, lungo in rettilineo
								CallFnc_VOID(SEG_EVENTS, EventBallKicked);
							}
						}
					}
				} else {
					// Non ha la palla: innesco della scivolata o furto ravvicinato su comando!
					if (trigger_pressed && LastTouchTeam != carrier_team && LastTouchTeam != 0xFF && Carrier->count == 0 && RestartType == 0 && Ball->count == 0) {
						bool opponent_has_ball = (g_is_ball_carried && LastTouchTeam != carrier_team);
						
						if (opponent_has_ball) {
							u16 c_dist_x = (LastTouchPlayer != 0xFF) ? ((Carrier->lx > SwSprite[LastTouchPlayer].lx) ? (Carrier->lx - SwSprite[LastTouchPlayer].lx) : (SwSprite[LastTouchPlayer].lx - Carrier->lx)) : 0xFFFF;
							u16 c_dist_y = (LastTouchPlayer != 0xFF) ? ((Carrier->ly > SwSprite[LastTouchPlayer].ly) ? (Carrier->ly - SwSprite[LastTouchPlayer].ly) : (SwSprite[LastTouchPlayer].ly - Carrier->ly)) : 0xFFFF;
							
							if ((dist_x <= 20 && dist_y <= 20) || (c_dist_x <= 16 && c_dist_y <= 16)) {
								// Contrasto in piedi immediato (spallata vincente)
								Ball->count = 30; // Immunità aumentata
								LastTouchTeam = carrier_team;
								LastTouchPlayer = carrier;
								if (Ball->anim > 3) Ball->anim = 3;
								Ball->frame = SPR_BALL_SIZE_1;
								g_pass_receiver = 0xFF;
							} else if (dist_x <= 36 && dist_y <= 24) {
								// Tackle orizzontale in scivolata
								Carrier->count = 30; // Scivolata + Cooldown per penalità miss
								Carrier->dx = (Ball->lx > Carrier->lx) ? 4 : -4;
								Carrier->dy = 0;
							}
						}
					}
				}
			}

		// 3. Esegui AI per tutti gli altri giocatori (movimento e tattica senza palla)
		for (u8 i = 0; i < 14; i++) {
			CallFnc_VOID_P1(SEG_LOGIC, PlayerAI, i);
		}

		// --- AGGIORNAMENTO ARBITRO ---
		CallFnc_VOID(SEG_GAMESTATE_8, UpdateReferee);
	} else {
		SwSprite[24].ly = 1000; // Nascondimento assoluto
		SwSprite[25].ly = 1000; // Nascondimento assoluto
		CallFnc_VOID_3PTR_U16(SEG_GAMESTATE_3, UpdateGameState_Restarts, game_state, wait_secs, start_sec, target_ly);
	}
}