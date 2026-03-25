using Microsoft.Win32;
using System.Text;

namespace WoWViewer
{
    public partial class SaveEditorForm : Form
    {
        //GAME = Game Save Name     0xC   -> 0x34   (max length 36 bytes)
        //WMAP = WarMap             0x34  -> 0x48
        //CALN = Calendar           0x48  -> 0x50
        //DtTm = Date Time          0x50  -> 0x60
        //HRSH = Human Research     0x60  -> 0x4D4
        //MRSH = Martian Research   0x4D4 -> 0x98C
        //MPAD = Movement Padded?   0x98C -> variable length x 10 entries
        //4 bytes = entry count = always 10
        //4 bytes = entry length including null terminator
        //entry string
        //4 padding bytes = 00 00 00 00 = accounts for MPAD = 16 byte loop
        //4 bytes = timestamp
        //4 bytes = entry length including null terminator
        //entry string
        //repeat for 10 entries total -> so 8 more
        //variable length block unknown
        //DMGL = 136 bytes length
        //SECTHUNIWMOB = sector 1

        //Unknown Blocks Remain

        //HCON = Construction
        //BMOL
        //4 bytes = unknown
        //4 bytes = building ID
        //ID Table
        //1      = Mark I Armoured Lorry                    = 01 00 = OTYPE_ArmouredLorry1
        //4      = Mark I Armoured Track Layer              = 04 00 = OTYPE_TrackLayer1
        //7      = Mark I Tunnelling Track Layer            = 07 00 = OTYPE_TunnellingV1
        //10     = Mark I Self Propelled Gun                = 0A 00 = OTYPE_SelfPropGun1
        //14     = Mark I Mobile Anti-Aircraft Array        = 0E 00 = OTYPE_AntiAircraftV1
        //17     = Sappers Lorry                            = 11 00 = OTYPE_Sappers1
        //19     = Formidable Class Ironclad                = 13 00 = OTYPE_Ironclad1
        //22     = Model A Submersible                      = 16 00 = OTYPE_Submarine1
        //25     = Observation Balloon                      = 19 00 = OTYPE_Balloon1
        //28     = Construction Vehicle                     = 1C 00 = OTYPE_ConstructionV1
        //30     = Mobile Repair Vehicle                    = 1E 00 = OTYPE_MobileRepairV1
        //32     = Mark I Mortar Bike                       = 20 00 = OTYPE_MortarBike1
        //35     = Vehicle Factory Level 1                  = 23 00 = OTYPE_VehicleFactory1
        //38     = Training Centre                          = 26 00 = OTYPE_TrainingCentre
        //39     = Ship Yard Level 1                        = 27 00 = OTYPE_ShipYard1
        //42     = Aircraft Hangar Level 1                  = 2A 00 = OTYPE_AirCraftHangar1
        //45     = Munitions Factory                        = 2D 00 = OTYPE_MunitionsFactory
        //46     = Construction Labs                        = 2E 00 = OTYPE_ConstructionLabs
        //47     = Steel Refinery                           = 2F 00 = OTYPE_SteelWorks
        //48     = Coal Mine                                = 30 00 = OTYPE_CoalMine
        //49     = Oil Refinery                             = 31 00 = OTYPE_OilRefinery
        //50     = 4.6 Inch Medium Gun Post                 = 32 00 = OTYPE_MediumGunEmp1
        //51     = MediumGunEmp2                            = 33 00 = OTYPE_MediumGunEmp2
        //52     = 6 Inch Heavy Gun Emplacement             = 34 00 = OTYPE_HeavyGunEmp1
        //53     = HeavyGunEmp2                             = 35 00 = OTYPE_HeavyGunEmp2
        //54     = HeavyGunEmp3                             = 36 00 = OTYPE_HeavyGunEmp3
        //55     = 1 Pdr Anti Aircraft Array                = 37 00 = OTYPE_AAGunEmplacement1
        //56     = AAGunEmplacement2                        = 38 00 = OTYPE_AAGunEmplacement2
        //57     = AAGunEmplacement3                        = 39 00 = OTYPE_AAGunEmplacement3
        //58     = Command Post                             = 3A 00 = OTYPE_CommandPost
        //59     = Railway Platform                         = 3B 00 = OTYPE_RailwayPlatform         CRASH
        //60     = Repair Workshop                          = 3C 00 = OTYPE_RepairWorkshops
        //62     = Scout Machine - 1st Gen                  = 3E 00 = OTYPE_ScoutM1
        //65     = Fighting Machine - 1st Gen               = 41 00 = OTYPE_FightingM1
        //68     = Tempest - 1st Gen                        = 44 00 = OTYPE_Tempest1
        //71     = Bombarding Machine - 1st Gen             = 47 00 = OTYPE_BombardingM1
        //74     = Electric Machine - 1st Gen               = 4A 00 = OTYPE_ElectricM1
        //77     = Flying Machine - 1st Gen                 = 4D 00 = OTYPE_FlyingM1
        //80     = Scanning Machine - 1st Gen               = 50 00 = OTYPE_ScanningM1
        //83     = Constrictor - 1st Gen                    = 53 00 = OTYPE_Constrictor1
        //86     = Xeno Telepath - 1st Gen                  = 56 00 = OTYPE_XenoTelepath1
        //89     = Handling Machine                         = 59 00 = OTYPE_HandlingM1
        //91     = Digging Mechanism                        = 5B 00 = OTYPE_DiggingM1
        //93     = Drone - 1st Gen                          = 5D 00 = OTYPE_Drone1
        //96     = Constructor Level 1                      = 60 00 = OTYPE_Constructor1
        //99     = Energy Weapon Plant                      = 63 00 = OTYPE_EnergyWeaponPlant
        //100    = Suspension Field Site Level 1            = 64 00 = OTYPE_SuspFieldSite1
        //103    = Telepathic Training Centre Level 1       = 67 00 = OTYPE_TelepathCentre1
        //106    = Biochemical Plant                        = 6A 00 = OTYPE_BioweapPlant
        //107    = Explosives Plant                         = 6B 00 = OTYPE_ExplosivesPlant
        //108    = Human Farm                               = 6C 00 = OTYPE_Farm
        //109    = Copper Forge                             = 6D 00 = OTYPE_CopperForge
        //110    = Heavy Element Plant                      = 6E 00 = OTYPE_HeavyElementPlant
        //111    = 12 KrK Rapid Heat Ray                    = 6F 00 = OTYPE_RapidHeatRay1
        //113    = 46 KrK Heat Ray Turret                   = 71 00 = OTYPE_HeatRayTurret1
        //116    = 102 DnO Projectile Launcher              = 74 00 = OTYPE_ProjectileLauncher1
        //119    = Power Plant                              = 77 00 = OTYPE_PowerPlant
        //120    = Communications Centre                    = 78 00 = OTYPE_CommsCentre
        //121    = Repair Facility                          = 79 00 = OTYPE_RepairFacility
        //122    = Matter Transfer Station                  = 7A 00 = OTYPE_MatterTransferStation
        //123    = BC SILO                                  = 7B 00 = OTYPE_BC_SILO
        //124    = BC SSILO                                 = 7C 00 = OTYPE_BC_SSILO
        //125    = BC DOME                                  = 7D 00 = OTYPE_BC_DOME
        //126    = BC PB1                                   = 7E 00 = OTYPE_BC_PB1
        //127    = BC PB2                                   = 7F 00 = OTYPE_BC_PB2
        //128    = BC PB3                                   = 80 00 = OTYPE_BC_PB3
        //129    = C Toe1                                   = 81 00 = OTYPE_C_Toe1
        //130    = C Toe2                                   = 82 00 = OTYPE_C_Toe2
        //131    = C Toe3                                   = 83 00 = OTYPE_C_Toe3
        //132    = C Toe4                                   = 84 00 = OTYPE_C_Toe4
        //133    = Con leg1                                 = 85 00 = OTYPE_Con_leg1
        //134    = Con leg2                                 = 86 00 = OTYPE_Con_leg2
        //135    = Con leg3                                 = 87 00 = OTYPE_Con_leg3
        //136    = Con leg4                                 = 88 00 = OTYPE_Con_leg4
        //137    = Con Stor                                 = 89 00 = OTYPE_Con_Stor
        //138    = Con X                                    = 8A 00 = OTYPE_Con_X
        //139    = CF Stor                                  = 8B 00 = OTYPE_CF_Stor
        //140    = CF Chim                                  = 8C 00 = OTYPE_CF_Chim
        //141    = EWP Gen                                  = 8D 00 = OTYPE_EWP_Gen
        //142    = EWP Gen2                                 = 8E 00 = OTYPE_EWP_Gen2
        //143    = EWP Gen3                                 = 8F 00 = OTYPE_EWP_Gen3
        //144    = EWP Gen4                                 = 90 00 = OTYPE_EWP_Gen4
        //145    = EWP END2                                 = 91 00 = OTYPE_EWP_END2
        //146    = EWP ENDV                                 = 92 00 = OTYPE_EWP_ENDV
        //147    = EWP EDV2                                 = 93 00 = OTYPE_EWP_EDV2
        //148    = EWP Bar1                                 = 94 00 = OTYPE_EWP_Bar1
        //149    = EWP Bar2                                 = 95 00 = OTYPE_EWP_Bar2
        //150    = EWP Bar3                                 = 96 00 = OTYPE_EWP_Bar3
        //151    = F arm1                                   = 97 00 = OTYPE_F_arm1
        //152    = F arm2                                   = 98 00 = OTYPE_F_arm2
        //153    = F arm3                                   = 99 00 = OTYPE_F_arm3
        //154    = F Stor                                   = 9A 00 = OTYPE_F_Stor
        //155    = F Pod                                    = 9B 00 = OTYPE_F_Pod
        //156    = He gun                                   = 9C 00 = OTYPE_He_gun
        //157    = He Mpodz                                 = 9D 00 = OTYPE_He_Mpodz
        //158    = He Eye                                   = 9E 00 = OTYPE_He_Eye
        //159    = He pod1                                  = 9F 00 = OTYPE_He_pod1
        //160    = He pod2                                  = A0 00 = OTYPE_He_pod2
        //161    = He pod3                                  = A1 00 = OTYPE_He_pod3
        //162    = He pod4                                  = A2 00 = OTYPE_He_pod4
        //163    = HX Ring                                  = A3 00 = OTYPE_HX_Ring
        //164    = Hx Pod1                                  = A4 00 = OTYPE_Hx_Pod1
        //165    = Hx Pod11                                 = A5 00 = OTYPE_Hx_Pod11
        //166    = Hx pod12                                 = A6 00 = OTYPE_Hx_pod12
        //167    = Hx Pod2                                  = A7 00 = OTYPE_Hx_Pod2
        //168    = Hx Pod21                                 = A8 00 = OTYPE_Hx_Pod21
        //169    = Hx pod22                                 = A9 00 = OTYPE_Hx_pod22
        //170    = Hx Pod3                                  = AA 00 = OTYPE_Hx_Pod3
        //171    = Hx Pod31                                 = AB 00 = OTYPE_Hx_Pod31
        //172    = Hx pod32                                 = AC 00 = OTYPE_Hx_pod32
        //173    = MT leg1                                  = AD 00 = OTYPE_MT_leg1
        //174    = MT leg2                                  = AE 00 = OTYPE_MT_leg2
        //175    = MT leg3                                  = AF 00 = OTYPE_MT_leg3
        //176    = MT leg4                                  = B0 00 = OTYPE_MT_leg4
        //177    = MT mainb                                 = B1 00 = OTYPE_MT_mainb
        //178    = MT maint                                 = B2 00 = OTYPE_MT_maint
        //179    = Pod                                      = B3 00 = OTYPE_Pod                     CRASH
        //180    = Pp Endp1                                 = B4 00 = OTYPE_Pp_Endp1
        //181    = Pp Endp2                                 = B5 00 = OTYPE_Pp_Endp2
        //182    = Pp Endp3                                 = B6 00 = OTYPE_Pp_Endp3
        //183    = Pp Endp4                                 = B7 00 = OTYPE_Pp_Endp4
        //184    = Pp Endp5                                 = B8 00 = OTYPE_Pp_Endp5
        //185    = Pp Endp6                                 = B9 00 = OTYPE_Pp_Endp6
        //186    = Pp Endp7                                 = BA 00 = OTYPE_Pp_Endp7
        //187    = Pp Endp8                                 = BB 00 = OTYPE_Pp_Endp8
        //188    = Pp Eye                                   = BC 00 = OTYPE_Pp_Eye
        //189    = Rf doors                                 = BD 00 = OTYPE_Rf_doors
        //190    = SF Spad                                  = BE 00 = OTYPE_SF_Spad
        //191    = SF Lpad                                  = BF 00 = OTYPE_SF_Lpad
        //192    = SF Arm1                                  = C0 00 = OTYPE_SF_Arm1
        //193    = SF Arm2                                  = C1 00 = OTYPE_SF_Arm2
        //194    = SF Arm3                                  = C2 00 = OTYPE_SF_Arm3
        //195    = TC talt1                                 = C3 00 = OTYPE_TC_talt1
        //196    = TC talt2                                 = C4 00 = OTYPE_TC_talt2
        //197    = TC midt1                                 = C5 00 = OTYPE_TC_midt1
        //198    = TC Wing1                                 = C6 00 = OTYPE_TC_Wing1
        //199    = TC Wing3                                 = C7 00 = OTYPE_TC_Wing3
        //200    = TC talt3                                 = C8 00 = OTYPE_TC_talt3
        //201    = TC midt2                                 = C9 00 = OTYPE_TC_midt2
        //202    = TC Wing2                                 = CA 00 = OTYPE_TC_Wing2
        //207    = CTreeCirc1                               = CF 00 = OTYPE_CTreeCirc1
        //208    = CTreeCirc2                               = D0 00 = OTYPE_CTreeCirc2
        //209    = CTreeCirc3                               = D1 00 = OTYPE_CTreeCirc3
        //210    = DTreeCirc1                               = D2 00 = OTYPE_DTreeCirc1
        //211    = DTreeCirc2                               = D3 00 = OTYPE_DTreeCirc2
        //212    = DTreeCirc3                               = D4 00 = OTYPE_DTreeCirc3
        //213    = CTree1                                   = D5 00 = OTYPE_CTree1
        //214    = CTree2                                   = D6 00 = OTYPE_CTree2
        //215    = CTree3                                   = D7 00 = OTYPE_CTree3
        //216    = CTree4                                   = D8 00 = OTYPE_CTree4
        //217    = CTree5                                   = D9 00 = OTYPE_CTree5
        //218    = CTree6                                   = DA 00 = OTYPE_CTree6
        //219    = CTree7                                   = DB 00 = OTYPE_CTree7
        //220    = DTree1                                   = DC 00 = OTYPE_DTree1
        //221    = DTree2                                   = DD 00 = OTYPE_DTree2
        //222    = DTree3                                   = DE 00 = OTYPE_DTree3
        //223    = DTree4                                   = DF 00 = OTYPE_DTree4
        //224    = DTree5                                   = E0 00 = OTYPE_DTree5
        //225    = DTree6                                   = E1 00 = OTYPE_DTree6
        //226    = DTree7                                   = E2 00 = OTYPE_DTree7
        //227    = DTree8                                   = E3 00 = OTYPE_DTree8
        //228    = DTree9                                   = E4 00 = OTYPE_DTree9
        //229    = DTree10                                  = E5 00 = OTYPE_DTree10
        //230    = VHall                                    = E6 00 = OTYPE_VHall
        //231    = WaterMill                                = E7 00 = OTYPE_WaterMill
        //232    = Chapel                                   = E8 00 = OTYPE_Chapel
        //233    = ShopOne                                  = E9 00 = OTYPE_ShopOne
        //234    = PubOne                                   = EA 00 = OTYPE_PubOne
        //235    = PubTwo                                   = EB 00 = OTYPE_PubTwo
        //236    = Barn                                     = EC 00 = OTYPE_Barn
        //237    = HouseTwo                                 = ED 00 = OTYPE_HouseTwo
        //238    = Cottage                                  = EE 00 = OTYPE_Cottage
        //239    = FarmHouse                                = EF 00 = OTYPE_FarmHouse
        //240    = SchoolOne                                = F0 00 = OTYPE_SchoolOne
        //241    = Abbey                                    = F1 00 = OTYPE_Abbey
        //242    = Castle                                   = F2 00 = OTYPE_Castle
        //243    = Crane                                    = F3 00 = OTYPE_Crane
        //244    = Church                                   = F4 00 = OTYPE_Church
        //245    = Corhut                                   = F5 00 = OTYPE_Corhut
        //246    = Dockyard                                 = F6 00 = OTYPE_Dockyard
        //247    = House1                                   = F7 00 = OTYPE_House1
        //248    = Cathedral                                = F8 00 = OTYPE_Cathedral
        //249    = Lighthouse                               = F9 00 = OTYPE_Lighthouse
        //250    = Manor                                    = FA 00 = OTYPE_Manor
        //251    = Terrace1                                 = FB 00 = OTYPE_Terrace1
        //252    = Terrace2                                 = FC 00 = OTYPE_Terrace2
        //253    = Londnstb                                 = FD 00 = OTYPE_Londnstb
        //254    = Timberhouse                              = FE 00 = OTYPE_Timberhouse
        //255    = Windmill                                 = FF 00 = OTYPE_Windmill
        //256    = CottonFactory                            = 00 01 = OTYPE_CottonFactory
        //257    = CottonChim                               = 01 01 = OTYPE_CottonChim
        //258    = Londnost                                 = 02 01 = OTYPE_Londnost
        //259    = Londnst1                                 = 03 01 = OTYPE_Londnst1
        //260    = Ldnost30                                 = 04 01 = OTYPE_Ldnost30
        //261    = Ldnost31                                 = 05 01 = OTYPE_Ldnost31
        //262    = Ldnost60                                 = 06 01 = OTYPE_Ldnost60
        //263    = Ldnost61                                 = 07 01 = OTYPE_Ldnost61
        //264    = Lonnstst                                 = 08 01 = OTYPE_Lonnstst
        //265    = Londst                                   = 09 01 = OTYPE_Londst
        //266    = Londst1                                  = 0A 01 = OTYPE_Londst1
        //267    = Londstst                                 = 0B 01 = OTYPE_Londstst
        //268    = Shop1r l                                 = 0C 01 = OTYPE_Shop1r_l
        //269    = Shop1u d                                 = 0D 01 = OTYPE_Shop1u_d
        //270    = Shop2r l                                 = 0E 01 = OTYPE_Shop2r_l
        //271    = Shop2u d                                 = 0F 01 = OTYPE_Shop2u_d
        //272    = StreetLamp                               = 10 01 = OTYPE_StreetLamp
        //273    = Monument                                 = 11 01 = OTYPE_Monument
        //274    = NStone1                                  = 12 01 = OTYPE_NStone1
        //275    = NStone2                                  = 13 01 = OTYPE_NStone2
        //276    = NStone3                                  = 14 01 = OTYPE_NStone3
        //277    = NStone4                                  = 15 01 = OTYPE_NStone4
        //278    = Parliament                               = 16 01 = OTYPE_Parliament
        //279    = Stonecircle                              = 17 01 = OTYPE_Stonecircle
        //281    = Ah shedr                                 = 19 01 = OTYPE_Ah_shedr
        //282    = Ah tower                                 = 1A 01 = OTYPE_Ah_tower
        //284    = CM 1rf                                   = 1C 01 = OTYPE_CM_1rf
        //285    = Cm Coal                                  = 1D 01 = OTYPE_Cm_Coal
        //286    = Cm Pass                                  = 1E 01 = OTYPE_Cm_Pass
        //287    = Cm Shaft                                 = 1F 01 = OTYPE_Cm_Shaft
        //288    = Cp main                                  = 20 01 = OTYPE_Cp_main
        //289    = Cp pitt                                  = 21 01 = OTYPE_Cp_pitt
        //291    = C L Roof                                 = 23 01 = OTYPE_C_L_Roof
        //293    = C L silo                                 = 25 01 = OTYPE_C_L_silo
        //294    = C L tank                                 = 26 01 = OTYPE_C_L_tank
        //298    = MC 2                                     = 2A 01 = OTYPE_MC_2
        //299    = MC Chim                                  = 2B 01 = OTYPE_MC_Chim
        //302    = OR Gas                                   = 2E 01 = OTYPE_OR_Gas
        //304    = OR Ubend                                 = 30 01 = OTYPE_OR_Ubend
        //306    = SR 1rf                                   = 32 01 = OTYPE_SR_1rf
        //307    = SR Burn                                  = 33 01 = OTYPE_SR_Burn
        //309    = TC 2                                     = 35 01 = OTYPE_TC_2
        //312    = VF 1r                                    = 38 01 = OTYPE_VF_1r
        //313    = Vf 2                                     = 39 01 = OTYPE_Vf_2
        //314    = Vf 2r                                    = 3A 01 = OTYPE_Vf_2r
        //315    = SH CAN 1                                 = 3B 01 = OTYPE_SH_CAN_1
        //316    = SH CIR 2                                 = 3C 01 = OTYPE_SH_CIR_2
        //317    = SH CIR 3                                 = 3D 01 = OTYPE_SH_CIR_3
        //318    = SH DO1 1                                 = 3E 01 = OTYPE_SH_DO1_1
        //319    = SH DO2 1                                 = 3F 01 = OTYPE_SH_DO2_1
        //320    = SH DRO 1                                 = 40 01 = OTYPE_SH_DRO_1
        //321    = SH LOC 1                                 = 41 01 = OTYPE_SH_LOC_1
        //322    = SH MOB 1                                 = 42 01 = OTYPE_SH_MOB_1
        //323    = SH OVA 2                                 = 43 01 = OTYPE_SH_OVA_2
        //324    = SH REC 1                                 = 44 01 = OTYPE_SH_REC_1
        //325    = SH ROL 1                                 = 45 01 = OTYPE_SH_ROL_1
        //326    = SH SUP 1                                 = 46 01 = OTYPE_SH_SUP_1
        //327    = SH WOS 1                                 = 47 01 = OTYPE_SH_WOS_1
        //328    = SH CIR 4                                 = 48 01 = OTYPE_SH_CIR_4
        //329    = SH TUN 1                                 = 49 01 = OTYPE_SH_TUN_1
        //330    = SH AIR 1                                 = 4A 01 = OTYPE_SH_AIR_1
        //331    = BSHADOW 1                                = 4B 01 = OTYPE_BSHADOW_1
        //332    = BSHADOW 2                                = 4C 01 = OTYPE_BSHADOW_2
        //333    = BSHADOW 3                                = 4D 01 = OTYPE_BSHADOW_3
        //334    = HEADLIGHT 1                              = 4E 01 = OTYPE_HEADLIGHT_1
        //335    = HEADLIGHT 2                              = 4F 01 = OTYPE_HEADLIGHT_2
        //336    = HEADLIGHT 3                              = 50 01 = OTYPE_HEADLIGHT_3
        //337    = EXPLOLIGHT 1                             = 51 01 = OTYPE_EXPLOLIGHT_1
        //338    = EXPLOLIGHT 2                             = 52 01 = OTYPE_EXPLOLIGHT_2
        //339    = EXPLOLIGHT 3                             = 53 01 = OTYPE_EXPLOLIGHT_3
        //340    = EXPLOLIGHT 4                             = 54 01 = OTYPE_EXPLOLIGHT_4
        //341    = EXPLOLIGHT 5                             = 55 01 = OTYPE_EXPLOLIGHT_5
        //342    = EXPLOLIGHT 6                             = 56 01 = OTYPE_EXPLOLIGHT_6
        //343    = EXPLOLIGHT 7                             = 57 01 = OTYPE_EXPLOLIGHT_7
        //344    = EXPLOLIGHT 8                             = 58 01 = OTYPE_EXPLOLIGHT_8
        //345    = EXPLOLIGHT 9                             = 59 01 = OTYPE_EXPLOLIGHT_9
        //346    = EXPLOLIGHT 10                            = 5A 01 = OTYPE_EXPLOLIGHT_10
        //347    = EXPLOSION PART 1A                        = 5B 01 = OTYPE_EXPLOSION_PART_1A
        //348    = EXPLOSION PART 1B                        = 5C 01 = OTYPE_EXPLOSION_PART_1B
        //349    = EXPLOSION PART 2A                        = 5D 01 = OTYPE_EXPLOSION_PART_2A
        //350    = EXPLOSION PART 2B                        = 5E 01 = OTYPE_EXPLOSION_PART_2B
        //351    = EXPLOSION PART 3A                        = 5F 01 = OTYPE_EXPLOSION_PART_3A
        //352    = EXPLOSION PART 3B                        = 60 01 = OTYPE_EXPLOSION_PART_3B
        //353    = EXPLOSION PART 4A                        = 61 01 = OTYPE_EXPLOSION_PART_4A
        //354    = EXPLOSION PART 4B                        = 62 01 = OTYPE_EXPLOSION_PART_4B
        //355    = EXPLOSION PART 5A                        = 63 01 = OTYPE_EXPLOSION_PART_5A
        //356    = EXPLOSION PART 5B                        = 64 01 = OTYPE_EXPLOSION_PART_5B
        //357    = EXPLOSION PART 6A                        = 65 01 = OTYPE_EXPLOSION_PART_6A
        //358    = EXPLOSION PART 6B                        = 66 01 = OTYPE_EXPLOSION_PART_6B
        //359    = EXPLOSION PART 7A                        = 67 01 = OTYPE_EXPLOSION_PART_7A
        //360    = EXPLOSION PART 7B                        = 68 01 = OTYPE_EXPLOSION_PART_7B
        //361    = EXPLOSION PART 8A                        = 69 01 = OTYPE_EXPLOSION_PART_8A
        //362    = EXPLOSION PART 8B                        = 6A 01 = OTYPE_EXPLOSION_PART_8B
        //363    = EXPLOSION PART 9A                        = 6B 01 = OTYPE_EXPLOSION_PART_9A
        //364    = EXPLOSION PART 9B                        = 6C 01 = OTYPE_EXPLOSION_PART_9B
        //365    = EXPLOSION PART 10A                       = 6D 01 = OTYPE_EXPLOSION_PART_10A
        //366    = EXPLOSION PART 10B                       = 6E 01 = OTYPE_EXPLOSION_PART_10B
        //367    = SMOKE 1                                  = 6F 01 = OTYPE_SMOKE_1
        //368    = SMOKE 2                                  = 70 01 = OTYPE_SMOKE_2
        //369    = SMOKE 3                                  = 71 01 = OTYPE_SMOKE_3
        //370    = SMOKE 4                                  = 72 01 = OTYPE_SMOKE_4
        //371    = SMOKE 5                                  = 73 01 = OTYPE_SMOKE_5
        //372    = SMOKE 6                                  = 74 01 = OTYPE_SMOKE_6
        //373    = SMOKE 7                                  = 75 01 = OTYPE_SMOKE_7
        //374    = SMOKE 8                                  = 76 01 = OTYPE_SMOKE_8
        //375    = SMOKE 9                                  = 77 01 = OTYPE_SMOKE_9
        //376    = SMOKE 10                                 = 78 01 = OTYPE_SMOKE_10
        //377    = HEATRAY TIP                              = 79 01 = OTYPE_HEATRAY_TIP
        //378    = FLYING DIRT                              = 7A 01 = OTYPE_FLYING_DIRT
        //379    = TREESTUMP                                = 7B 01 = OTYPE_TREESTUMP
        //380    = MOVE ACK                                 = 7C 01 = OTYPE_MOVE_ACK
        //381    = Explosion 1                              = 7D 01 = OTYPE_Explosion_1
        //382    = Explosion 2                              = 7E 01 = OTYPE_Explosion_2
        //383    = Explosion 3                              = 7F 01 = OTYPE_Explosion_3
        //384    = Explosion 4                              = 80 01 = OTYPE_Explosion_4
        //385    = Explosion 5                              = 81 01 = OTYPE_Explosion_5
        //386    = Explosion 6                              = 82 01 = OTYPE_Explosion_6
        //387    = Explosion 7                              = 83 01 = OTYPE_Explosion_7
        //388    = Explosion 8                              = 84 01 = OTYPE_Explosion_8
        //389    = Explosion 9                              = 85 01 = OTYPE_Explosion_9
        //390    = Explosion 10                             = 86 01 = OTYPE_Explosion_10
        //391    = Explosion 1 Sea                          = 87 01 = OTYPE_Explosion_1_Sea
        //392    = Explosion 2 Sea                          = 88 01 = OTYPE_Explosion_2_Sea
        //393    = Explosion 1 Air                          = 89 01 = OTYPE_Explosion_1_Air
        //394    = Explosion 2 Air                          = 8A 01 = OTYPE_Explosion_2_Air
        //396    = 130 DnO Canister                         = 8C 01 = OTYPE_130_DnO_Canister
        //407    = BlackDust Launcher1                      = 97 01 = OTYPE_BlackDust_Launcher1
        //408    = BlackDust Launcher2                      = 98 01 = OTYPE_BlackDust_Launcher2
        //409    = BlackDust Launcher3                      = 99 01 = OTYPE_BlackDust_Launcher3
        //410    = Glue Jet1                                = 9A 01 = OTYPE_Glue_Jet1
        //411    = Glue Jet2                                = 9B 01 = OTYPE_Glue_Jet2
        //412    = Glue Jet3                                = 9C 01 = OTYPE_Glue_Jet3
        //413    = 1 Pounder pom pom                        = 9D 01 = OTYPE_1_Pounder_pom_pom
        //414    = 2 Pounder pom pom                        = 9E 01 = OTYPE_2_Pounder_pom_pom
        //415    = 3 Pounder gun                            = 9F 01 = OTYPE_3_Pounder_gun
        //416    = 6 Pounder gun                            = A0 01 = OTYPE_6_Pounder_gun
        //417    = 13 Pounder gun                           = A1 01 = OTYPE_13_Pounder_gun
        //418    = 18 Pounder gun                           = A2 01 = OTYPE_18_Pounder_gun
        //419    = 3 9 Inch Naval gun                       = A3 01 = OTYPE_3_9_Inch_Naval_gun
        //420    = 4 6 Inch Naval gun                       = A4 01 = OTYPE_4_6_Inch_Naval_gun
        //421    = 6 Inch Naval gun                         = A5 01 = OTYPE_6_Inch_Naval_gun
        //422    = 9 2 Inch Naval gun                       = A6 01 = OTYPE_9_2_Inch_Naval_gun
        //423    = 12 Inch Naval gun                        = A7 01 = OTYPE_12_Inch_Naval_gun
        //424    = 4 6 Inch howitzer                        = A8 01 = OTYPE_4_6_Inch_howitzer
        //425    = 6 Inch howitzer                          = A9 01 = OTYPE_6_Inch_howitzer
        //426    = 8 Inch howitzer                          = AA 01 = OTYPE_8_Inch_howitzer
        //427    = 12 KrK Rapid Heat Ray                    = AB 01 = OTYPE_12_KrK_Rapid_Heat_Ray
        //428    = 18 KrK Rapid Heat Ray                    = AC 01 = OTYPE_18_KrK_Rapid_Heat_Ray
        //429    = 38 KrK Heat Ray                          = AD 01 = OTYPE_38_KrK_Heat_Ray
        //430    = 46 KrK Heat Ray                          = AE 01 = OTYPE_46_KrK_Heat_Ray
        //431    = 62 KrK UV Ray                            = AF 01 = OTYPE_62_KrK_UV_Ray
        //432    = 80 KrK X Ray                             = B0 01 = OTYPE_80_KrK_X_Ray
        //433    = 102 DnO Canister Launcher                = B1 01 = OTYPE_102_DnO_Canister_Launcher
        //434    = 130 DnO Canister Launcher                = B2 01 = OTYPE_130_DnO_Canister_Launcher
        //435    = 165 DnO Canister Launcher                = B3 01 = OTYPE_165_DnO_Canister_Launcher
        //436    = 188 DnO Canister Launcher                = B4 01 = OTYPE_188_DnO_Canister_Launcher
        //437    = EMR Charge Pulser1                       = B5 01 = OTYPE_EMR_Charge_Pulser1
        //438    = EMR Charge Pulser2                       = B6 01 = OTYPE_EMR_Charge_Pulser2
        //439    = EMR Charge Pulser3                       = B7 01 = OTYPE_EMR_Charge_Pulser3
        //440    = Handlers                                 = B8 01 = OTYPE_Handlers
        //441    = 1 Pdr AA Pom Pom                         = B9 01 = OTYPE_1_Pdr_AA_Pom_Pom
        //442    = 2 Pdr AA Pom Pom                         = BA 01 = OTYPE_2_Pdr_AA_Pom_Pom
        //443    = 3 Pdr AA Gun                             = BB 01 = OTYPE_3_Pdr_AA_Gun
        //444    = Bridge Military                          = BC 01 = OTYPE_Bridge_Military
        //445    = Bridge Country                           = BD 01 = OTYPE_Bridge_Country
        //446    = Bridge City                              = BE 01 = OTYPE_Bridge_City
        //447    = Bridge Railway                           = BF 01 = OTYPE_Bridge_Railway
        //448    = BridgePart Railway                       = C0 01 = OTYPE_BridgePart_Railway
        //451    = Wire fence                               = C3 01 = OTYPE_Wire_Fence
        //454    = Electric Fencing                         = C6 01 = OTYPE_Electric_Fencing
        //457    = 2Pd Mortar                               = C9 01 = OTYPE_2Pd_Mortar
        //458    = 3Pd Mortar                               = CA 01 = OTYPE_3Pd_Mortar
        //459    = 6Pd Mortar                               = CB 01 = OTYPE_6Pd_Mortar
        //460    = BridgePart Military                      = CC 01 = OTYPE_BridgePart_Military
        //461    = BridgePart Country                       = CD 01 = OTYPE_BridgePart_Country
        //462    = BridgePart City                          = CE 01 = OTYPE_BridgePart_City
        //465    = Electric Fencing                         = D1 01 = OTYPE_Electric_Fencing
        //466    = Laser Fencing                            = D2 01 = OTYPE_Laser_Fencing
        //467    = Plasma Fencing                           = D3 01 = OTYPE_Plasma_Fencing
        //468    = AH dock                                  = D4 01 = OTYPE_AH_dock
        //470    = OR Chim                                  = D6 01 = OTYPE_OR_Chim
        //472    = MF FacR                                  = D8 01 = OTYPE_MF_FacR
        //473    = MF tunn                                  = D9 01 = OTYPE_MF_tunn
        //474    = MF bunk                                  = DA 01 = OTYPE_MF_bunk
        //475    = MF bunkd                                 = DB 01 = OTYPE_MF_bunkd
        //476    = SY towr                                  = DC 01 = OTYPE_SY_towr
        //477    = Sy side                                  = DD 01 = OTYPE_Sy_side
        //478    = Sy side2                                 = DE 01 = OTYPE_Sy_side2
        //479    = Sy roof                                  = DF 01 = OTYPE_Sy_roof
        //480    = Sy gate                                  = E0 01 = OTYPE_Sy_gate
        //481    = Sy gate2                                 = E1 01 = OTYPE_Sy_gate2
        //482    = Sy boat                                  = E2 01 = OTYPE_Sy_boat
        //483    = Sy eflor                                 = E3 01 = OTYPE_Sy_eflor
        //484    = Explosion 11                             = E4 01 = OTYPE_Explosion_11
        //485    = Explosion 12                             = E5 01 = OTYPE_Explosion_12
        //486    = Explosion 13                             = E6 01 = OTYPE_Explosion_13
        //487    = Explosion 14                             = E7 01 = OTYPE_Explosion_14
        //488    = Explosion 15                             = E8 01 = OTYPE_Explosion_15
        //489    = Explosion 16                             = E9 01 = OTYPE_Explosion_16
        //490    = Explosion 17                             = EA 01 = OTYPE_Explosion_17
        //491    = Explosion 18                             = EB 01 = OTYPE_Explosion_18
        //492    = Explosion 19                             = EC 01 = OTYPE_Explosion_19
        //493    = Explosion 20                             = ED 01 = OTYPE_Explosion_20
        //494    = Explosion 21                             = EE 01 = OTYPE_Explosion_21
        //495    = Explosion 22                             = EF 01 = OTYPE_Explosion_22
        //496    = Explosion 23                             = F0 01 = OTYPE_Explosion_23
        //497    = Explosion 24                             = F1 01 = OTYPE_Explosion_24
        //498    = Explosion 25                             = F2 01 = OTYPE_Explosion_25
        //499    = Explosion 26                             = F3 01 = OTYPE_Explosion_26
        //500    = Explosion 27                             = F4 01 = OTYPE_Explosion_27
        //501    = Explosion 28                             = F5 01 = OTYPE_Explosion_28
        //502    = Explosion 29                             = F6 01 = OTYPE_Explosion_29
        //503    = Explosion 30                             = F7 01 = OTYPE_Explosion_30
        //504    = Explosion 31                             = F8 01 = OTYPE_Explosion_31
        //505    = Explosion 32                             = F9 01 = OTYPE_Explosion_32
        //506    = Explosion 33                             = FA 01 = OTYPE_Explosion_33
        //507    = Explosion 34                             = FB 01 = OTYPE_Explosion_34
        //508    = Explosion 35                             = FC 01 = OTYPE_Explosion_35
        //509    = Explosion 36                             = FD 01 = OTYPE_Explosion_36
        //510    = Explosion 37                             = FE 01 = OTYPE_Explosion_37
        //511    = Explosion 38                             = FF 01 = OTYPE_Explosion_38
        //512    = Explosion 39                             = 00 02 = OTYPE_Explosion_39
        //513    = Explosion 40                             = 01 02 = OTYPE_Explosion_40
        //514    = Explosion 3 Sea                          = 02 02 = OTYPE_Explosion_3_Sea
        //515    = Explosion 4 Sea                          = 03 02 = OTYPE_Explosion_4_Sea
        //516    = Explosion 5 Sea                          = 04 02 = OTYPE_Explosion_5_Sea
        //517    = Explosion 6 Sea                          = 05 02 = OTYPE_Explosion_6_Sea
        //518    = Explosion 7 Sea                          = 06 02 = OTYPE_Explosion_7_Sea
        //519    = Explosion 8 Sea                          = 07 02 = OTYPE_Explosion_8_Sea
        //520    = Explosion 9 Sea                          = 08 02 = OTYPE_Explosion_9_Sea
        //521    = Explosion 10 Sea                         = 09 02 = OTYPE_Explosion_10_Sea
        //522    = Explosion 11 Sea                         = 0A 02 = OTYPE_Explosion_11_Sea
        //523    = Explosion 12 Sea                         = 0B 02 = OTYPE_Explosion_12_Sea
        //524    = Explosion 13 Sea                         = 0C 02 = OTYPE_Explosion_13_Sea
        //525    = Explosion 14 Sea                         = 0D 02 = OTYPE_Explosion_14_Sea
        //526    = Explosion 15 Sea                         = 0E 02 = OTYPE_Explosion_15_Sea
        //527    = Explosion 16 Sea                         = 0F 02 = OTYPE_Explosion_16_Sea
        //528    = Explosion 17 Sea                         = 10 02 = OTYPE_Explosion_17_Sea
        //529    = Explosion 18 Sea                         = 11 02 = OTYPE_Explosion_18_Sea
        //530    = Explosion 19 Sea                         = 12 02 = OTYPE_Explosion_19_Sea
        //531    = Explosion 20 Sea                         = 13 02 = OTYPE_Explosion_20_Sea
        //532    = Explosion 21 Sea                         = 14 02 = OTYPE_Explosion_21_Sea
        //533    = Explosion 22 Sea                         = 15 02 = OTYPE_Explosion_22_Sea
        //534    = Explosion 23 Sea                         = 16 02 = OTYPE_Explosion_23_Sea
        //535    = Explosion 24 Sea                         = 17 02 = OTYPE_Explosion_24_Sea
        //536    = Explosion 25 Sea                         = 18 02 = OTYPE_Explosion_25_Sea
        //537    = Explosion 26 Sea                         = 19 02 = OTYPE_Explosion_26_Sea
        //538    = Explosion 27 Sea                         = 1A 02 = OTYPE_Explosion_27_Sea
        //539    = Explosion 28 Sea                         = 1B 02 = OTYPE_Explosion_28_Sea
        //540    = Explosion 29 Sea                         = 1C 02 = OTYPE_Explosion_29_Sea
        //541    = Explosion 30 Sea                         = 1D 02 = OTYPE_Explosion_30_Sea
        //542    = Explosion 31 Sea                         = 1E 02 = OTYPE_Explosion_31_Sea
        //543    = Explosion 32 Sea                         = 1F 02 = OTYPE_Explosion_32_Sea
        //544    = Explosion 33 Sea                         = 20 02 = OTYPE_Explosion_33_Sea
        //545    = Explosion 34 Sea                         = 21 02 = OTYPE_Explosion_34_Sea
        //546    = Explosion 35 Sea                         = 22 02 = OTYPE_Explosion_35_Sea
        //547    = Explosion 36 Sea                         = 23 02 = OTYPE_Explosion_36_Sea
        //548    = Explosion 37 Sea                         = 24 02 = OTYPE_Explosion_37_Sea
        //549    = Explosion 38 Sea                         = 25 02 = OTYPE_Explosion_38_Sea
        //550    = Explosion 39 Sea                         = 26 02 = OTYPE_Explosion_39_Sea
        //551    = Explosion 40 Sea                         = 27 02 = OTYPE_Explosion_40_Sea
        //552    = Explosion 3 Air                          = 28 02 = OTYPE_Explosion_3_Air
        //553    = Explosion 4 Air                          = 29 02 = OTYPE_Explosion_4_Air
        //554    = Explosion 5 Air                          = 2A 02 = OTYPE_Explosion_5_Air
        //555    = Explosion 6 Air                          = 2B 02 = OTYPE_Explosion_6_Air
        //556    = Explosion 7 Air                          = 2C 02 = OTYPE_Explosion_7_Air
        //557    = Explosion 8 Air                          = 2D 02 = OTYPE_Explosion_8_Air
        //558    = Explosion 9 Air                          = 2E 02 = OTYPE_Explosion_9_Air
        //559    = Explosion 10 Air                         = 2F 02 = OTYPE_Explosion_10_Air
        //560    = Explosion 11 Air                         = 30 02 = OTYPE_Explosion_11_Air
        //561    = Explosion 12 Air                         = 31 02 = OTYPE_Explosion_12_Air
        //562    = Explosion 13 Air                         = 32 02 = OTYPE_Explosion_13_Air
        //563    = Explosion 14 Air                         = 33 02 = OTYPE_Explosion_14_Air
        //564    = Explosion 15 Air                         = 34 02 = OTYPE_Explosion_15_Air
        //565    = Explosion 16 Air                         = 35 02 = OTYPE_Explosion_16_Air
        //566    = Explosion 17 Air                         = 36 02 = OTYPE_Explosion_17_Air
        //567    = Explosion 18 Air                         = 37 02 = OTYPE_Explosion_18_Air
        //568    = Explosion 19 Air                         = 38 02 = OTYPE_Explosion_19_Air
        //569    = Explosion 20 Air                         = 39 02 = OTYPE_Explosion_20_Air
        //570    = Explosion 21 Air                         = 3A 02 = OTYPE_Explosion_21_Air
        //571    = Explosion 22 Air                         = 3B 02 = OTYPE_Explosion_22_Air
        //572    = Explosion 23 Air                         = 3C 02 = OTYPE_Explosion_23_Air
        //573    = Explosion 24 Air                         = 3D 02 = OTYPE_Explosion_24_Air
        //574    = Explosion 25 Air                         = 3E 02 = OTYPE_Explosion_25_Air
        //575    = Explosion 26 Air                         = 3F 02 = OTYPE_Explosion_26_Air
        //576    = Explosion 27 Air                         = 40 02 = OTYPE_Explosion_27_Air
        //577    = Explosion 28 Air                         = 41 02 = OTYPE_Explosion_28_Air
        //578    = Explosion 29 Air                         = 42 02 = OTYPE_Explosion_29_Air
        //579    = Explosion 30 Air                         = 43 02 = OTYPE_Explosion_30_Air
        //580    = Explosion 31 Air                         = 44 02 = OTYPE_Explosion_31_Air
        //581    = Explosion 32 Air                         = 45 02 = OTYPE_Explosion_32_Air
        //582    = Explosion 33 Air                         = 46 02 = OTYPE_Explosion_33_Air
        //583    = Explosion 34 Air                         = 47 02 = OTYPE_Explosion_34_Air
        //584    = Explosion 35 Air                         = 48 02 = OTYPE_Explosion_35_Air
        //585    = Explosion 36 Air                         = 49 02 = OTYPE_Explosion_36_Air
        //586    = Explosion 37 Air                         = 4A 02 = OTYPE_Explosion_37_Air
        //587    = Explosion 38 Air                         = 4B 02 = OTYPE_Explosion_38_Air
        //588    = Explosion 39 Air                         = 4C 02 = OTYPE_Explosion_39_Air
        //589    = Explosion 40 Air                         = 4D 02 = OTYPE_Explosion_40_Air
        //590    = Death Explosion 1                        = 4E 02 = OTYPE_Death_Explosion_1
        //591    = Death Explosion 2                        = 4F 02 = OTYPE_Death_Explosion_2
        //592    = Death Explosion 3                        = 50 02 = OTYPE_Death_Explosion_3
        //593    = Death Explosion 4                        = 51 02 = OTYPE_Death_Explosion_4
        //594    = Death Explosion 5                        = 52 02 = OTYPE_Death_Explosion_5
        //595    = Death Explosion 6                        = 53 02 = OTYPE_Death_Explosion_6
        //596    = Death Explosion 7                        = 54 02 = OTYPE_Death_Explosion_7
        //597    = Death Explosion 8                        = 55 02 = OTYPE_Death_Explosion_8
        //627    = Mines                                    = 73 02 = OTYPE_Mines
        //686    = Ambient Sea1                             = AE 02 = OTYPE_Ambient_Sea1
        //687    = Ambient Sea2                             = AF 02 = OTYPE_Ambient_Sea2
        //688    = Ambient Mountain                         = B0 02 = OTYPE_Ambient_Mountain
        //689    = Ambient Field                            = B1 02 = OTYPE_Ambient_Field
        //690    = Ambient Factory                          = B2 02 = OTYPE_Ambient_Factory
        //691    = Ambient City                             = B3 02 = OTYPE_Ambient_City
        //692    = Ambient Docks                            = B4 02 = OTYPE_Ambient_Docks
        //693    = Ambient Wind                             = B5 02 = OTYPE_Ambient_Wind
        //695    = Wire Fence                               = B7 02 = OTYPE_Wire_Fence
        //696    = CPMarker                                 = B8 02 = OTYPE_CPMarker
        //697    = Explosives 1                             = B9 02 = OTYPE_Explosives_1
        //698    = Explosives 2                             = BA 02 = OTYPE_Explosives_2
        //699    = Explosives 3                             = BB 02 = OTYPE_Explosives_3
        //700    = Mind Fk 1                                = BC 02 = OTYPE_Mind_Fk_1
        //701    = Mind Fk 2                                = BD 02 = OTYPE_Mind_Fk_2
        //702    = Mind Fk 3                                = BE 02 = OTYPE_Mind_Fk_3
        //706    = Shock Bomb 1                             = C2 02 = OTYPE_Shock_Bomb_1
        //707    = Shock Bomb 2                             = C3 02 = OTYPE_Shock_Bomb_2
        //708    = Shock Bomb 3                             = C4 02 = OTYPE_Shock_Bomb_3
        //712    = Shock Round 1                            = C8 02 = OTYPE_Shock_Round_1
        //715    = Explosion 41 Mind Fk 1                   = CB 02 = OTYPE_Explosion_41_Mind_Fk_1
        //716    = Explosion 42 Mind Fk 2                   = CC 02 = OTYPE_Explosion_42_Mind_Fk_2
        //717    = Explosion 43 Mind Fk 3                   = CD 02 = OTYPE_Explosion_43_Mind_Fk_3
        //718    = Explosion 44 Shock Bomb 1                = CE 02 = OTYPE_Explosion_44_Shock_Bomb_1
        //719    = Explosion 45 Shock Bomb 2                = CF 02 = OTYPE_Explosion_45_Shock_Bomb_2
        //720    = Explosion 46 Shock Bomb 3                = D0 02 = OTYPE_Explosion_46_Shock_Bomb_3
        //721    = LH1 45F                                  = D1 02 = OTYPE_LH1_45F
        //722    = LH2 45F                                  = D2 02 = OTYPE_LH2_45F
        //723    = LH3 45F                                  = D3 02 = OTYPE_LH3_45F
        //724    = LH4 45F                                  = D4 02 = OTYPE_LH4_45F
        //725    = LH5 45F                                  = D5 02 = OTYPE_LH5_45F
        //726    = LH6 45F                                  = D6 02 = OTYPE_LH6_45F
        //727    = LH7 45F                                  = D7 02 = OTYPE_LH7_45F
        //728    = LH8 45F                                  = D8 02 = OTYPE_LH8_45F
        //729    = LH9 45F                                  = D9 02 = OTYPE_LH9_45F
        //730    = LH10 45F                                 = DA 02 = OTYPE_LH10_45F
        //731    = LH11 45F                                 = DB 02 = OTYPE_LH11_45F
        //732    = LH12 45F                                 = DC 02 = OTYPE_LH12_45F
        //733    = LH13 45F                                 = DD 02 = OTYPE_LH13_45F
        //735    = LH16 45F                                 = DF 02 = OTYPE_LH16_45F
        //736    = LH17 45F                                 = E0 02 = OTYPE_LH17_45F
        //737    = LH18 45F                                 = E1 02 = OTYPE_LH18_45F
        //738    = LH19 45F                                 = E2 02 = OTYPE_LH19_45F
        //739    = LH21 45F                                 = E3 02 = OTYPE_LH21_45F
        //740    = LH22 45F                                 = E4 02 = OTYPE_LH22_45F
        //741    = LH23 45F                                 = E5 02 = OTYPE_LH23_45F
        //751    = Ironclad 1 Turret                        = EF 02 = OTYPE_Ironclad_1_Turret
        //752    = Ironclad 2 Turret                        = F0 02 = OTYPE_Ironclad_2_Turret
        //753    = Ironclad 3 Turret                        = F1 02 = OTYPE_Ironclad_3_Turret
        //754    = Ironclad 1 Superstructure                = F2 02 = OTYPE_Ironclad_1_Superstructure
        //755    = Ironclad 2 Superstructure                = F3 02 = OTYPE_Ironclad_2_Superstructure
        //756    = Ironclad 3 Superstructure                = F4 02 = OTYPE_Ironclad_3_Superstructure
        //788    = Explosion 47 Explosives1                 = 14 03 = OTYPE_Explosion_47_Explosives1
        //789    = Explosion 48 Explosives2                 = 15 03 = OTYPE_Explosion_48_Explosives2
        //790    = Explosion 49 Explosives3                 = 16 03 = OTYPE_Explosion_49_Explosives3
        //800    = INFILTRATE1                              = 20 03 = OTYPE_INFILTRATE1
        //801    = INFILTRATE2                              = 21 03 = OTYPE_INFILTRATE2
        //802    = INFILTRATE3                              = 22 03 = OTYPE_INFILTRATE3
        //803    = FREEZE1                                  = 23 03 = OTYPE_FREEZE1
        //804    = FREEZE2                                  = 24 03 = OTYPE_FREEZE2
        //805    = FREEZE3                                  = 25 03 = OTYPE_FREEZE3
        //806    = SCARE1                                   = 26 03 = OTYPE_SCARE1
        //807    = SCARE2                                   = 27 03 = OTYPE_SCARE2
        //808    = SCARE3                                   = 28 03 = OTYPE_SCARE3
        //809    = CONTROLBRAIN1                            = 29 03 = OTYPE_CONTROLBRAIN1
        //810    = CONTROLBRAIN2                            = 2A 03 = OTYPE_CONTROLBRAIN2
        //811    = CONTROLBRAIN3                            = 2B 03 = OTYPE_CONTROLBRAIN3
        //812    = EXPLO INFILTRATE1                        = 2C 03 = OTYPE_EXPLO_INFILTRATE1
        //813    = EXPLO INFILTRATE2                        = 2D 03 = OTYPE_EXPLO_INFILTRATE2
        //814    = EXPLO INFILTRATE3                        = 2E 03 = OTYPE_EXPLO_INFILTRATE3
        //815    = EXPLO FREEZE1                            = 2F 03 = OTYPE_EXPLO_FREEZE1
        //816    = EXPLO FREEZE2                            = 30 03 = OTYPE_EXPLO_FREEZE2
        //817    = EXPLO FREEZE3                            = 31 03 = OTYPE_EXPLO_FREEZE3
        //818    = EXPLO SCARE1                             = 32 03 = OTYPE_EXPLO_SCARE1
        //819    = EXPLO SCARE2                             = 33 03 = OTYPE_EXPLO_SCARE2
        //820    = EXPLO SCARE3                             = 34 03 = OTYPE_EXPLO_SCARE3
        //821    = EXPLO CONTROLBRAIN1                      = 35 03 = OTYPE_EXPLO_CONTROLBRAIN1
        //822    = EXPLO CONTROLBRAIN2                      = 36 03 = OTYPE_EXPLO_CONTROLBRAIN2
        //823    = EXPLO CONTROLBRAIN3                      = 37 03 = OTYPE_EXPLO_CONTROLBRAIN3
        //824    = HB PARL2                                 = 38 03 = OTYPE_HB_PARL2
        //825    = HB PARL3                                 = 39 03 = OTYPE_HB_PARL3
        //826    = HB PARL4                                 = 3A 03 = OTYPE_HB_PARL4
        //827    = HB PARL5                                 = 3B 03 = OTYPE_HB_PARL5
        //828    = HB PARL6                                 = 3C 03 = OTYPE_HB_PARL6
        //829    = HB PARL7                                 = 3D 03 = OTYPE_HB_PARL7
        //830    = Martian Base                             = 3E 03 = OTYPE_Martian_Base
        //831    = MB HARM1                                 = 3F 03 = OTYPE_MB_HARM1
        //832    = MB HARM2                                 = 40 03 = OTYPE_MB_HARM2
        //833    = MB HARM3                                 = 41 03 = OTYPE_MB_HARM3
        //834    = MB HARM4                                 = 42 03 = OTYPE_MB_HARM4
        //835    = MB LARM1                                 = 43 03 = OTYPE_MB_LARM1
        //836    = MB LARM2                                 = 44 03 = OTYPE_MB_LARM2
        //837    = MB LARM3                                 = 45 03 = OTYPE_MB_LARM3
        //838    = MB LARM4                                 = 46 03 = OTYPE_MB_LARM4
        //839    = MB MFOT1                                 = 47 03 = OTYPE_MB_MFOT1
        //840    = MB MFOT2                                 = 48 03 = OTYPE_MB_MFOT2
        //841    = MB MFOT3                                 = 49 03 = OTYPE_MB_MFOT3
        //842    = MB MFOT4                                 = 4A 03 = OTYPE_MB_MFOT4
        //843    = MB IPOD                                  = 4B 03 = OTYPE_MB_IPOD
        //844    = MB PETAL                                 = 4C 03 = OTYPE_MB_PETAL
        //845    = LH1 00F                                  = 4D 03 = OTYPE_LH1_00F
        //846    = LH2 00F                                  = 4E 03 = OTYPE_LH2_00F
        //847    = LH3 00F                                  = 4F 03 = OTYPE_LH3_00F
        //848    = LH4 00F                                  = 50 03 = OTYPE_LH4_00F
        //849    = LH5 00F                                  = 51 03 = OTYPE_LH5_00F
        //850    = LH6 00F                                  = 52 03 = OTYPE_LH6_00F
        //851    = LH7 00F                                  = 53 03 = OTYPE_LH7_00F
        //852    = LH8 00F                                  = 54 03 = OTYPE_LH8_00F
        //853    = LH9 00F                                  = 55 03 = OTYPE_LH9_00F
        //854    = LH10 00F                                 = 56 03 = OTYPE_LH10_00F
        //855    = LH11 00F                                 = 57 03 = OTYPE_LH11_00F
        //856    = LH12 00F                                 = 58 03 = OTYPE_LH12_00F
        //857    = LH13 00F                                 = 59 03 = OTYPE_LH13_00F
        //858    = LH16 00F                                 = 5A 03 = OTYPE_LH16_00F
        //859    = LH17 00F                                 = 5B 03 = OTYPE_LH17_00F
        //860    = LH18 00F                                 = 5C 03 = OTYPE_LH18_00F
        //861    = LH19 00F                                 = 5D 03 = OTYPE_LH19_00F
        //862    = LH21 00F                                 = 5E 03 = OTYPE_LH21_00F
        //863    = LH22 00F                                 = 5F 03 = OTYPE_LH22_00F
        //864    = LH23 00F                                 = 60 03 = OTYPE_LH23_00F
        //865    = LH1 30F                                  = 61 03 = OTYPE_LH1_30F
        //866    = LH2 30F                                  = 62 03 = OTYPE_LH2_30F
        //867    = LH3 30F                                  = 63 03 = OTYPE_LH3_30F
        //868    = LH4 30F                                  = 64 03 = OTYPE_LH4_30F
        //869    = LH5 30F                                  = 65 03 = OTYPE_LH5_30F
        //870    = LH6 30F                                  = 66 03 = OTYPE_LH6_30F
        //871    = LH7 30F                                  = 67 03 = OTYPE_LH7_30F
        //872    = LH8 30F                                  = 68 03 = OTYPE_LH8_30F
        //873    = LH9 30F                                  = 69 03 = OTYPE_LH9_30F
        //874    = LH10 30F                                 = 6A 03 = OTYPE_LH10_30F
        //875    = LH11 30F                                 = 6B 03 = OTYPE_LH11_30F
        //876    = LH12 30F                                 = 6C 03 = OTYPE_LH12_30F
        //877    = LH13 30F                                 = 6D 03 = OTYPE_LH13_30F
        //878    = LH16 30F                                 = 6E 03 = OTYPE_LH16_30F
        //879    = LH17 30F                                 = 6F 03 = OTYPE_LH17_30F
        //880    = LH18 30F                                 = 70 03 = OTYPE_LH18_30F
        //881    = LH19 30F                                 = 71 03 = OTYPE_LH19_30F
        //882    = LH21 30F                                 = 72 03 = OTYPE_LH21_30F
        //883    = LH22 30F                                 = 73 03 = OTYPE_LH22_30F
        //884    = LH23 30F                                 = 74 03 = OTYPE_LH23_30F
        //885    = LH1 60F                                  = 75 03 = OTYPE_LH1_60F
        //886    = LH2 60F                                  = 76 03 = OTYPE_LH2_60F
        //887    = LH3 60F                                  = 77 03 = OTYPE_LH3_60F
        //888    = LH4 60F                                  = 78 03 = OTYPE_LH4_60F
        //889    = LH5 60F                                  = 79 03 = OTYPE_LH5_60F
        //890    = LH6 60F                                  = 7A 03 = OTYPE_LH6_60F
        //891    = LH7 60F                                  = 7B 03 = OTYPE_LH7_60F
        //892    = LH8 60F                                  = 7C 03 = OTYPE_LH8_60F
        //893    = LH9 60F                                  = 7D 03 = OTYPE_LH9_60F
        //894    = LH10 60F                                 = 7E 03 = OTYPE_LH10_60F
        //895    = LH11 60F                                 = 7F 03 = OTYPE_LH11_60F
        //896    = LH12 60F                                 = 80 03 = OTYPE_LH12_60F
        //897    = LH13 60F                                 = 81 03 = OTYPE_LH13_60F
        //898    = LH16 60F                                 = 82 03 = OTYPE_LH16_60F
        //899    = LH17 60F                                 = 83 03 = OTYPE_LH17_60F
        //900    = LH18 60F                                 = 84 03 = OTYPE_LH18_60F
        //901    = LH19 60F                                 = 85 03 = OTYPE_LH19_60F
        //902    = LH21 60F                                 = 86 03 = OTYPE_LH21_60F
        //903    = LH22 60F                                 = 87 03 = OTYPE_LH22_60F
        //904    = LH23 60F                                 = 88 03 = OTYPE_LH23_60F
        //909    = CP sb1                                   = 8D 03 = OTYPE_CP_sb1
        //910    = CP sb2                                   = 8E 03 = OTYPE_CP_sb2
        //911    = CP sb3                                   = 8F 03 = OTYPE_CP_sb3
        //912    = CP sb4                                   = 90 03 = OTYPE_CP_sb4
        //913    = Cp tinh                                  = 91 03 = OTYPE_Cp_tinh
        //914    = CON ARMS                                 = 92 03 = OTYPE_CON_ARMS
        //915    = BlackDust Canister 1                     = 93 03 = OTYPE_BlackDust_Canister_1
        //916    = BlackDust Canister 2                     = 94 03 = OTYPE_BlackDust_Canister_2
        //917    = BlackDust Canister 3                     = 95 03 = OTYPE_BlackDust_Canister_3
        //918    = BlackDust 1                              = 96 03 = OTYPE_BlackDust_1
        //919    = BlackDust 2                              = 97 03 = OTYPE_BlackDust_2
        //920    = BlackDust 3                              = 98 03 = OTYPE_BlackDust_3
        //921    = RedTree1                                 = 99 03 = OTYPE_RedTree1
        //922    = RedTree2                                 = 9A 03 = OTYPE_RedTree2
        //923    = RedTree3                                 = 9B 03 = OTYPE_RedTree3
        //924    = RedTree4                                 = 9C 03 = OTYPE_RedTree4
        //925    = RedTree5                                 = 9D 03 = OTYPE_RedTree5
        //926    = RedTree6                                 = 9E 03 = OTYPE_RedTree6
        //927    = RedTree7                                 = 9F 03 = OTYPE_RedTree7
        //928    = RedTree8                                 = A0 03 = OTYPE_RedTree8
        //929    = RedTree9                                 = A1 03 = OTYPE_RedTree9
        //930    = RedTree10                                = A2 03 = OTYPE_RedTree10
        //931    = RedTree11                                = A3 03 = OTYPE_RedTree11
        //932    = RedTree12                                = A4 03 = OTYPE_RedTree12
        //933    = RedTree13                                = A5 03 = OTYPE_RedTree13
        //934    = RedTree14                                = A6 03 = OTYPE_RedTree14
        //935    = RedTree15                                = A7 03 = OTYPE_RedTree15
        //936    = RedTree16                                = A8 03 = OTYPE_RedTree16
        //937    = RedTree17                                = A9 03 = OTYPE_RedTree17
        //938    = HighEnergyCapacitorShellLauncher         = AA 03 = OTYPE_HighEnergyCapacitorShellLauncher
        //939    = Neutraliser                              = AB 03 = OTYPE_Neutraliser
        //940    = AMB REDWEED                              = AC 03 = OTYPE_AMB_REDWEED
        //942    = AMB STREAM                               = AE 03 = OTYPE_AMB_STREAM
        //946    = Drone Death Explosion1                   = B2 03 = OTYPE_Drone_Death_Explosion1
        //947    = Drone Death Explosion2                   = B3 03 = OTYPE_Drone_Death_Explosion2
        //948    = Drone Death Explosion3                   = B4 03 = OTYPE_Drone_Death_Explosion3
        //949    = Drone Target1                            = B5 03 = OTYPE_Drone_Target1
        //950    = Drone Target2                            = B6 03 = OTYPE_Drone_Target2
        //951    = Drone Target3                            = B7 03 = OTYPE_Drone_Target3
        //952    = WATEREXPLOSION 1A                        = B8 03 = OTYPE_WATEREXPLOSION_1A
        //953    = WATEREXPLOSION 1B                        = B9 03 = OTYPE_WATEREXPLOSION_1B
        //954    = WATEREXPLOSION 2A                        = BA 03 = OTYPE_WATEREXPLOSION_2A
        //955    = WATEREXPLOSION 2B                        = BB 03 = OTYPE_WATEREXPLOSION_2B
        //956    = WATEREXPLOSION 3A                        = BC 03 = OTYPE_WATEREXPLOSION_3A
        //957    = WATEREXPLOSION 3B                        = BD 03 = OTYPE_WATEREXPLOSION_3B
        //958    = WATEREXPLOSION 4A                        = BE 03 = OTYPE_WATEREXPLOSION_4A
        //959    = WATEREXPLOSION 4B                        = BF 03 = OTYPE_WATEREXPLOSION_4B
        //960    = WATEREXPLOSION 5A                        = C0 03 = OTYPE_WATEREXPLOSION_5A
        //961    = WATEREXPLOSION 5B                        = C1 03 = OTYPE_WATEREXPLOSION_5B
        //967    = WATER RIPPLE 1                           = C7 03 = OTYPE_WATER_RIPPLE_1
        //968    = WATER RIPPLE 2                           = C8 03 = OTYPE_WATER_RIPPLE_2
        //969    = WATER RIPPLE 3                           = C9 03 = OTYPE_WATER_RIPPLE_3
        //970    = WATER TWINKLE 1                          = CA 03 = OTYPE_WATER_TWINKLE_1
        //971    = WATER TWINKLE 2                          = CB 03 = OTYPE_WATER_TWINKLE_2
        //972    = WATER TWINKLE 3                          = CC 03 = OTYPE_WATER_TWINKLE_3
        //973    = LH1 90F                                  = CD 03 = OTYPE_LH1_90F
        //974    = LH3 90F                                  = CE 03 = OTYPE_LH3_90F
        //975    = LH4 90F                                  = CF 03 = OTYPE_LH4_90F
        //976    = LH7 90F                                  = D0 03 = OTYPE_LH7_90F
        //977    = LH8 90F                                  = D1 03 = OTYPE_LH8_90F
        //978    = LH10 90F                                 = D2 03 = OTYPE_LH10_90F
        //979    = LH11 90F                                 = D3 03 = OTYPE_LH11_90F
        //980    = LH12 90F                                 = D4 03 = OTYPE_LH12_90F
        //981    = LH13 90F                                 = D5 03 = OTYPE_LH13_90F
        //982    = LH17 90F                                 = D6 03 = OTYPE_LH17_90F
        //983    = LH19 90F                                 = D7 03 = OTYPE_LH19_90F
        //984    = LH22 90F                                 = D8 03 = OTYPE_LH22_90F
        //985    = LH23 90F                                 = D9 03 = OTYPE_LH23_90F
        //986    = PODARRIVALEXPLO                          = DA 03 = OTYPE_PODARRIVALEXPLO
        //987    = EXITARROW                                = DB 03 = OTYPE_EXITARROW
        //988    = SUB SPLASH                               = DC 03 = OTYPE_SUB_SPLASH
        //989    = SUB SPLASH PART                          = DD 03 = OTYPE_SUB_SPLASH_PART
        //990    = TUN DIRT                                 = DE 03 = OTYPE_TUN_DIRT
        //991    = TUN DIRT PART                            = DF 03 = OTYPE_TUN_DIRT_PART
        //992    = CONSTRIC GUNK                            = E0 03 = OTYPE_CONSTRIC_GUNK
        //993    = CONSTRIC GUNK PART                       = E1 03 = OTYPE_CONSTRIC_GUNK_PART
        //994    = SFX AMB Forest                           = E2 03 = OTYPE_SFX_AMB_Forest
        //995    = SFX AMB River                            = E3 03 = OTYPE_SFX_AMB_River
        //996    = SFX AMB Brook                            = E4 03 = OTYPE_SFX_AMB_Brook
        //997    = EXPLOSION COL1                           = E5 03 = OTYPE_EXPLOSION_COL1
        //998    = EXPLOSION COL2                           = E6 03 = OTYPE_EXPLOSION_COL2
        //999    = EXPLOSION COL3                           = E7 03 = OTYPE_EXPLOSION_COL3
        //1000   = SHORTFLAME1                              = E8 03 = OTYPE_SHORTFLAME1
        //1002   = AIRBOMB                                  = EA 03 = OTYPE_AIRBOMB
        //1003   = AIRBOMB1                                 = EB 03 = OTYPE_AIRBOMB1
        //1004   = AIRBOMB2                                 = EC 03 = OTYPE_AIRBOMB2
        //1005   = AIRBOMB3                                 = ED 03 = OTYPE_AIRBOMB3
        //1006   = EXPLOSION AIRBOMB1                       = EE 03 = OTYPE_EXPLOSION_AIRBOMB1
        //1007   = EXPLOSION AIRBOMB2                       = EF 03 = OTYPE_EXPLOSION_AIRBOMB2
        //1008   = EXPLOSION AIRBOMB3                       = F0 03 = OTYPE_EXPLOSION_AIRBOMB3
        //SELO
        //Other Four Byte Tags
        //BAMO
        //BATP
        //VEHI
        //VEHU
        //AUNI
        //WMOB
        //HCON
        //WMOB
        //BATP
        //ACON
        //HRES
        //ARES
        //DMGL

        private List<WowTextEntry> entries = new List<WowTextEntry>();
        private static readonly Encoding Latin1 = Encoding.GetEncoding("iso-8859-1");
        private int entryCount = 1397; // there are only 0-1396 entries
        private const int NAME_OFFSET = 0x0C;
        private const int TIME_OFFSET = 0x4C;
        private const int DATE_OFFSET = 0x5A;
        //private DateTime HUMAN_RESPONSE = new DateTime(1898, 9, 7); // human response date // not used due to swap save file ability
        private DateTime MARTIAN_INVASION = new DateTime(1898, 9, 1); // martian invasion date
        // MARTIAN_INVASION is used as the default lower bound for the date time picker unless overridden or mismatching
        private DateTime DATE_LIMIT = new DateTime(1753, 1, 1); // date limit
        private string fileName = ""; // selected file name
        private WowSaveEntry selectedSave = new WowSaveEntry(); // selected save file settings
        private bool isHuman = false;
        private byte[][] sectorData = new byte[30][];
        private int selectedSectorIndex = -1;
        private RegistryKey tweakKey = Registry.LocalMachine.OpenSubKey(@"SOFTWARE\WOW6432Node\Rage\Jeff Wayne's 'The War Of The Worlds'\1.00.000\Tweak", true)!;
        private int unitsValue;
        private int boatsValue;
        public SaveEditorForm()
        {
            InitializeComponent();
            InitializeSaveLoader();
            ToolTip tooltip = new ToolTip();
            ToolTipHelper.EnableTooltips(this.Controls, tooltip, typeof(Label));
            listBox1.SelectedIndexChanged += listBox1_SelectedIndexChanged!;
            listBox2.SelectedIndexChanged += listBox2_SelectedIndexChanged!;
            listBox3.SelectedIndexChanged += listBox3_SelectedIndexChanged!;
            listBox4.SelectedIndexChanged += listBox4_SelectedIndexChanged!;
            // parse TEXT.ojd
            if (!File.Exists("TEXT.ojd"))
            {
                MessageBox.Show("TEXT.ojd not found! Please check the installation!"); // file doesn't exist
                this.Close();
                return;
            }
            // max units / boats as read from the registry
            unitsValue = Convert.ToInt32(tweakKey.GetValue("Max units in sector"));
            boatsValue = Convert.ToInt32(tweakKey.GetValue("Max boats in sector"));
            //
            byte[] data = File.ReadAllBytes("TEXT.ojd"); // read the file into a byte array
            switch (data.Length) // check file size
            {
                case 63839: // english  - 63839 bytes
                case 75224: // french   - 75224 bytes
                case 70448: // german   - 70448 bytes
                case 70218: // italian  - 70218 bytes
                case 71617: // spanish  - 71617 bytes
                    entryCount = 1396; // support for the original TEXT.ojd file without the added Credits entry.
                    break;
            }
            int offset = 0x289; // first string starts at 0x289
            for (int i = 0; i < entryCount; i++) // there are only 1396 entries
            {
                byte category = data[offset + 4];  // Faction: 00 = Martian, 01 = Human, 02 = UI
                ushort length = (ushort)(data[offset + 8] | (data[offset + 9] << 8)); // bytes 9 and 10 are the string length
                int stringOffset = offset + 10; // string offset
                string text = Latin1.GetString(data, stringOffset, length - 1).Replace("\\n", "\n");
                // string length is one less than the ushort length as length contains the null operator // replaces \n with actual new line
                entries.Add(new WowTextEntry { Name = text, Faction = category, Index = (ushort)i });
                offset += (int)length + 9; // move offset to next entry // not + 10 because length contains the null operator ( hence - 1 above at text )
            }
        }
        // initialize save loader and count saves
        private void InitializeSaveLoader() { for (int i = 1; i <= 5; i++) { SaveLoader("Human", i); } for (int i = 1; i <= 5; i++) { SaveLoader("Martian", i); } }
        // save loader
        private void SaveLoader(string type, int number) { if (File.Exists("SaveGame\\" + $"{type}.00{number}")) { listBox1.Items.Add($"{type}.00{number}"); } }
        // save file button
        private void button1_Click(object sender, EventArgs e)
        {
            if (!fileSafetyCheck()) { return; }
            using (var fs = new FileStream(fileName, FileMode.Open, FileAccess.Write))
            {
                // only write values that have changed
                if (textBox1.Text != selectedSave.Name)
                {
                    byte[] nameBytes = new byte[36];
                    byte[] newNameBytes = Encoding.ASCII.GetBytes(textBox1.Text);
                    Array.Copy(newNameBytes, nameBytes, Math.Min(36, newNameBytes.Length));
                    fs.Seek(NAME_OFFSET, SeekOrigin.Begin);
                    fs.Write(nameBytes, 0, 36);
                    selectedSave.Name = textBox1.Text; // update the selected save object
                }
                if (dateTimePicker1.Value != selectedSave.dateTime || checkBox2.Checked)
                {
                    DateTime dt = dateTimePicker1.Value;
                    // Preserve the existing elapsed-days portion, only replace time-of-day.
                    byte[] existingTickBytes = new byte[4];
                    fs.Seek(TIME_OFFSET, SeekOrigin.Begin);
                    fs.Read(existingTickBytes, 0, 4);
                    float existingTick = BitConverter.ToSingle(existingTickBytes, 0);
                    const float tpd = 24f * 20.055f;
                    float dayBase = (float)Math.Floor(existingTick / tpd) * tpd;
                    float newTodTicks = (dt.Hour + dt.Minute / 60f + dt.Second / 3600f) * 20.055f;
                    float newTick = dayBase + newTodTicks;
                    fs.Seek(TIME_OFFSET, SeekOrigin.Begin);
                    fs.Write(BitConverter.GetBytes(newTick), 0, 4);
                    ushort day = (ushort)(dt.Day - 1);
                    ushort month = (ushort)(dt.Month - 1);
                    ushort year = checkBox2.Checked ? (ushort)numericUpDown1.Value : (ushort)dt.Year;
                    fs.Seek(DATE_OFFSET, SeekOrigin.Begin);
                    fs.Write(BitConverter.GetBytes(day), 0, 2);
                    fs.Write(BitConverter.GetBytes(month), 0, 2);
                    fs.Write(BitConverter.GetBytes(year), 0, 2);
                    selectedSave.dateTime = dt;
                }
            }
            MessageBox.Show("Save Game Updated!");
            label3.Text = "Status : Changes Saved"; // update the status label
            button1.Enabled = false; // disable the save button
        }
        // the save file selected in list box index changed
        private void listBox1_SelectedIndexChanged(object sender, EventArgs e)
        {
            // remove event handlers for the controls
            textBox1.TextChanged -= AnyControlChanged!;
            dateTimePicker1.ValueChanged -= AnyControlChanged!;
            checkBox1.CheckedChanged -= checkBox1_CheckedChanged!;
            checkBox2.CheckedChanged -= checkBox2_CheckedChanged!;
            // parse the save file which adds the event handlers back
            parseSaveFile();
        }
        // parse the save file
        private void parseSaveFile()
        {
            if (!fileSafetyCheck()) { return; }

            byte[] saveData = File.ReadAllBytes(fileName);
            string saveName = Latin1.GetString(saveData, 0x0C, 36).Split('\0')[0];
            textBox1.Text = saveName;
            selectedSave.Name = saveName;
            float tickFloat = BitConverter.ToSingle(saveData, TIME_OFFSET);
            const float TicksPerDay = 24f * 20.055f;
            float totalHours = tickFloat % TicksPerDay / 20.055f;
            int hours = Math.Clamp((int)totalHours, 0, 23);
            int minutes = Math.Clamp((int)((totalHours - hours) * 60), 0, 59);
            int seconds = Math.Clamp((int)(((totalHours - hours) * 60 - minutes) * 60), 0, 59);
            ushort day = BitConverter.ToUInt16(saveData, DATE_OFFSET);
            day += 1; // update to account for zero-based indexing
            ushort month = BitConverter.ToUInt16(saveData, DATE_OFFSET + 2);
            month += 1; // update to account for zero-based indexing
            ushort year = BitConverter.ToUInt16(saveData, DATE_OFFSET + 4);

            if (year < 1753)
            {
                numericUpDown1.Value = year; // set numeric up down to minimum date
                selectedSave.actualYear = year; // set the actual year to the selected year
                year = 1753; // set to minimum date
                checkBox2.Checked = true; // override enabled
                checkBox2.Enabled = true; // enable the year override checkbox
                numericUpDown1.Enabled = true; // enable the year override numeric up down
            }
            else
            {
                checkBox2.Checked = false; // override disabled
                checkBox2.Enabled = false; // disble the year override checkbox
                numericUpDown1.Enabled = false; // isdble the year override numeric up down
            }
            selectedSave.dateTime = new DateTime(year, month, day, hours, minutes, seconds);

            // 10 bytes??

            // HRSH     Horsell? Hours Seconds? Hands?

            // 12 FF bytes
            // 2 00 Bytes
            // 4 FF Bytes
            byte[] marker = Encoding.ASCII.GetBytes("SECTHUNIWMOB");
            var positions = new List<int>();
            for (int i = 0; i <= saveData.Length - marker.Length; i++)
            {
                bool match = true;
                for (int j = 0; j < marker.Length; j++)
                    if (saveData[i + j] != marker[j]) { match = false; break; }
                if (match) positions.Add(i);
            }
            for (int i = 0; i < positions.Count && i < 30; i++)
            {
                int begin = positions[i];
                int end = (i + 1 < positions.Count) ? positions[i + 1] : saveData.Length;
                sectorData[i] = saveData[begin..end];
            }
            //textBox2.Text = positions.Count.ToString();






            minimumDateCheck(selectedSave.dateTime); // check if the date is within the minimum date range
            dateTimePicker1.Value = selectedSave.dateTime; // set value after the date check
            textBox1.Enabled = true; // enable the text box
            dateTimePicker1.Enabled = true; // enable the date picker
            checkBox1.Enabled = true; // enable the checkbox
            button1.Enabled = false; // disables the save button when switching saves
            label3.Text = "Status : No Changes Made"; // update the status label
            button2.Enabled = true; // enable the swap sides button
            button3.Enabled = true; // enable the delete save button
            listBox2.Items.Clear(); // clear the sector list box
            isHuman = fileName.Contains("Human");
            label6.Text = isHuman ? $"{entries[193].Name} :" : $"{entries[196].Name} :"; // pulled from entries for localisation
            label7.Text = isHuman ? $"{entries[194].Name} :" : $"{entries[197].Name} :";
            label8.Text = isHuman ? $"{entries[195].Name} :" : $"{entries[198].Name} :";
            label9.Text = ""; // reset the sector label
            // parse the text file to get sector names
            int start = isHuman ? 1 : 32; // skip entry 0 (campaign name) in each faction group
            for (int i = 0; i < 30; i++) { listBox2.Items.Add(entries[start + i].Name); }
            // add event handlers for the controls
            textBox1.TextChanged += AnyControlChanged!;
            dateTimePicker1.ValueChanged += AnyControlChanged!;
            checkBox1.CheckedChanged += checkBox1_CheckedChanged!;
            checkBox2.CheckedChanged += checkBox2_CheckedChanged!;
        }
        // double check the save file exists incase deleted by the user while the program is open
        private bool fileSafetyCheck()
        {
            fileName = $"SaveGame\\{listBox1.SelectedItem}"; // update fileName for reading and writing
            if (!File.Exists(fileName))
            {
                MessageBox.Show("Where did the file go?"); // user deleted the file while the program was open
                listBox1.Items.Remove(listBox1.SelectedItem!); // remove the file from the list box
                textBox1.Text = ""; // clear the textbox
                textBox1.Enabled = false; // disable the text box
                dateTimePicker1.Enabled = false; // disables the date picker
                checkBox1.Enabled = false; // enable the checkbox
                button1.Enabled = false; // disables the save button
                button2.Enabled = false; // disables the swap sides button
                button3.Enabled = false; // disables the delete button
                return false; // return false if file doesn't exist after disabling UI elements
            }
            return true; // return true if the file still exists
        }
        // AnyControlChanged handles when controls are changed that do not need extra logic such as the override date limit checkbox
        // save name updated
        // current date updated
        private void AnyControlChanged(object sender, EventArgs e) { compareSaveValues(); }
        // override date limit
        private void checkBox1_CheckedChanged(object sender, EventArgs e)
        {
            dateTimePicker1.MinDate = checkBox1.Checked ? DATE_LIMIT : MARTIAN_INVASION;
            checkBox2.Enabled = checkBox1.Checked;
        }
        // minimum date check
        private void minimumDateCheck(DateTime compare)
        {
            if (compare < MARTIAN_INVASION)
            {
                dateTimePicker1.MinDate = DATE_LIMIT; // set the min date to 01/01/1753 ( current default )
                checkBox1.Checked = true; // override enabled
            }
            else
            {
                dateTimePicker1.MinDate = MARTIAN_INVASION; // set the min date to the martian invasion date due to swap save file ability
                checkBox1.Checked = false; // reset to default // HUMAN_RESPONSE date is no longer used
            }
        }
        // compare save values to see if any changes have been made
        private void compareSaveValues()
        {
            if (dateTimePicker1.Value != selectedSave.dateTime
                || textBox1.Text != selectedSave.Name || checkBox2.Checked
                )
            {
                button1.Enabled = true; // enable the save button
                label3.Text = "Status : Changes Made";
            }
            else
            {
                button1.Enabled = false; // disable the save button
                label3.Text = "Status : No Changes Made";
            }
        }
        // swap sides button ( rename Human.00# to Martian.00# and vice versa )
        private void button2_Click(object sender, EventArgs e)
        {
            if (!fileSafetyCheck()) { return; }
            DialogResult result = MessageBox.Show("Are you sure you want to swap sides on this save file?", "Swap Sides", MessageBoxButtons.YesNo);
            if (result == DialogResult.No) { return; }
            string opposite = "";
            if (fileName.Contains("Human")) { opposite = "Martian"; }
            else { opposite = "Human"; }
            for (int i = 1; i <= 5; i++)
            {
                if (!File.Exists($"SaveGame\\{opposite}.00{i}"))
                {
                    File.Move($"{fileName}", $"SaveGame\\{opposite}.00{i}");
                    reInitialize("Save Swapped!"); // reinitialize the save loader and repopulate the list box
                    return;
                }
            }
            MessageBox.Show("No space, please delete a save file on the opposing side!");
        }
        // delete save file button
        private void button3_Click(object sender, EventArgs e)
        {
            if (!fileSafetyCheck()) { return; }
            DialogResult result = MessageBox.Show("Are you sure you want to delete this save file?", "Delete Save File", MessageBoxButtons.YesNo);
            if (result == DialogResult.Yes)
            {
                File.Delete(fileName); // delete the file
                reInitialize("Save Deleted!"); // reinitialize the save loader and repopulate the list box
            }
        }
        private void reInitialize(string message)
        {
            MessageBox.Show(message);
            listBox1.Items.Clear(); // clear list box
            InitializeSaveLoader(); // reinitialize the save loader and repopulate the list box
            button2.Enabled = false; // disable the swap sides button
            button3.Enabled = false; // disable the delete save button
        }
        // override year limit checkbox
        private void checkBox2_CheckedChanged(object sender, EventArgs e)
        {
            numericUpDown1.Enabled = checkBox2.Checked;
            compareSaveValues();
        }
        // list box 2 selected index changed ( sector selection )
        private void listBox2_SelectedIndexChanged(object sender, EventArgs e)
        {
            selectedSectorIndex = listBox2.SelectedIndex;
            label9.Text = listBox2.Text; // update the label with the selected sector

            byte[] sec = sectorData[selectedSectorIndex];
            // Show: header line + first 256 bytes as hex + unparsed count
            var sb = new StringBuilder();
            sb.AppendLine($"Sector {selectedSectorIndex + 1}  |  {sec.Length} bytes total");
            sb.AppendLine();

            // Known header: SECTHUNIWMOB (12) + unit_count byte (1) + padding (3) + first_bmol_id (4) + SELO (4) + a (4) + b (4) + c (4) = 0x24 bytes
            bool hasPlayerUnits = sec[0x0C] != 0 || BitConverter.ToUInt32(sec, 0x14) != 0x494E5541; // 0x494E5541 = "AUNI"
            if (hasPlayerUnits)
            {
                int unit_count = sec[0x0C];
                uint first_bmol = BitConverter.ToUInt32(sec, 0x10);
                uint selo_a = BitConverter.ToUInt32(sec, 0x18);
                uint selo_b = BitConverter.ToUInt32(sec, 0x1C);
                uint selo_c = BitConverter.ToUInt32(sec, 0x20);
                sb.AppendLine($"  unit_count  = {unit_count}");
                sb.AppendLine($"  first_bmol  = {first_bmol}");
                sb.AppendLine($"  SELO a/b/c  = {selo_a} / {selo_b} / {selo_c}");
            }
            else
            {
                sb.AppendLine("  (no player units — enemy/neutral sector)");
            }
            sb.AppendLine();

            // Raw hex dump of first N bytes
            int dumpLen = sec.Length;
            for (int i = 0; i < dumpLen; i += 16)
            {
                sb.Append($"  {i:X4}: ");
                for (int j = 0; j < 16 && i + j < dumpLen; j++)
                    sb.Append($"{sec[i + j]:X2} ");
                sb.Append("  ");
                for (int j = 0; j < 16 && i + j < dumpLen; j++)
                    sb.Append(sec[i + j] >= 32 && sec[i + j] < 127 ? (char)sec[i + j] : '.');
                sb.AppendLine();
            }
            if (sec.Length > 512) sb.AppendLine($"  ... ({sec.Length - 512} more bytes)");

            richTextBox1.Text = sb.ToString(); // or whatever your textbox is named
        }
        // list box 3 selection ( building selection )
        private void listBox3_SelectedIndexChanged(object sender, EventArgs e)
        {

        }
        // list box 4 selection ( unit selection )
        private void listBox4_SelectedIndexChanged(object sender, EventArgs e)
        {

        }
    }
}