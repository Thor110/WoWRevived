namespace WoWLauncher
{
    /// <summary>
    /// Converts a stored value into a slider position without ever throwing.
    /// </summary>
    public static class TrackBarValue
    {
        public static int Clamp(double value, int minimum, int maximum)
        {
            if (double.IsNaN(value)) { return minimum; }
            return (int)Math.Clamp(Math.Round(value), minimum, maximum); // round before converting, (int)1.2 * 100 would be 100
        }
    }

    /// <summary>
    /// Moves a batch of files as a single operation: every move is checked up front, and if a move
    /// still fails part way the moves already made are undone, so assets are never left half swapped.
    /// </summary>
    public static class FileMoves
    {
        /// <summary>Returns null if the moves can all be made in order, otherwise a description of the problem.</summary>
        public static string? Validate(IReadOnlyList<(string From, string To)> moves, Func<string, bool> exists)
        {
            // simulate the moves in order so a file vacated by an earlier move can be a later destination
            var present = new Dictionary<string, bool>(StringComparer.OrdinalIgnoreCase);
            bool Has(string path) => present.TryGetValue(path, out bool value) ? value : exists(path);
            foreach (var (from, to) in moves)
            {
                if (!Has(from)) { return $"Missing file: {from}"; }
                if (Has(to)) { return $"File already exists: {to}"; }
                present[from] = false;
                present[to] = true;
            }
            return null;
        }

        public static bool TryMoveAll(IReadOnlyList<(string From, string To)> moves, out string? error)
        {
            error = Validate(moves, File.Exists);
            if (error != null) { return false; }
            var done = new List<(string From, string To)>();
            try
            {
                foreach (var move in moves)
                {
                    File.Move(move.From, move.To);
                    done.Add(move);
                }
                return true;
            }
            catch (Exception ex) when (ex is IOException || ex is UnauthorizedAccessException)
            {
                error = ex.Message;
                for (int i = done.Count - 1; i >= 0; i--) // undo in reverse order
                {
                    try { File.Move(done[i].To, done[i].From); }
                    catch (Exception undoEx) when (undoEx is IOException || undoEx is UnauthorizedAccessException)
                    {
                        error += $"\n\nCould not restore {done[i].From}: {undoEx.Message}";
                    }
                }
                return false;
            }
        }
    }

    /// <summary>
    /// Decides where the Enhanced Assets and Larger Minimaps files belong.
    ///
    /// Each managed file lives in exactly one place at a time, so the current state is read from
    /// which storage folder is missing a file, never from file sizes:
    ///   - Enhanced UI sprites: DAT\ when enabled, DAT-EXTRA\HD\ when disabled.
    ///   - Unit count sprites (hu-cnt24.spr, MA-CNT24.SPR) have two variants stored in DAT-EXTRA\HD\ (enhanced)
    ///     and DAT-EXTRA\UNITS\ (larger minimaps). DAT\ holds the larger variant if Larger Minimaps is on,
    ///     else the enhanced variant if Enhanced Assets is on, else neither (the game uses its archive copy).
    ///   - MAPS\MAPS.WoW is the larger map archive when MAPS\MAPS-Regular.WoW exists, and the regular one when
    ///     MAPS\MAPS-Larger.WoW exists.
    /// </summary>
    public static class AssetLayout
    {
        public const string Dat = "DAT";
        public const string HdStore = "DAT-EXTRA\\HD";
        public const string UnitsStore = "DAT-EXTRA\\UNITS";
        public const string Maps = "MAPS\\MAPS.WoW";
        public const string MapsRegular = "MAPS\\MAPS-Regular.WoW";
        public const string MapsLarger = "MAPS\\MAPS-Larger.WoW";
        public const string EnhancedMarker = "DAT\\cd_bd1.spr";

        public static readonly string[] UnitSprites = { "hu-cnt24.spr", "MA-CNT24.SPR" };

        public static readonly string[] EnhancedFiles = {
            "cd_bd1.spr", "cd_bd2.spr", "cd_bd3.spr", "cd_bd4.spr", "cd_bd5.spr", "cd_bd6.spr", "cd_bd7.spr",
            "HBBdown.spr", "HBBup.spr", "HBTdown.spr", "HBTup.spr", "HBuildB1.spr", "hexitarr.spr", "Hmessage.spr",
            "hmframe.spr", "hu-cnt32.spr", "hu-cnt48.spr", "hu_buton.spr", "hu_calen.spr",
            "hu_event.spr", "hu_speed.spr", "hu_twirl.spr", "MA-CNT32.SPR", "MA-CNT48.SPR", "MAN-BUT.SPR",
            "MA_BUTON.SPR", "MA_CALEN.SPR", "MA_EVENT.SPR", "ma_goo.spr", "MA_SPEED.SPR", "MA_WORLD.SPR", "MBBDOWN.SPR",
            "MBBUP.SPR", "MBTDOWN.SPR", "MBTUP.SPR", "MBUILDB1.SPR", "MBUILDB2.SPR", "MCOG.SPR", "mexitarr.spr", "MFACT.SPR",
            "MMFRAME.SPR", "MRESRCHB.SPR", "mr_exit.spr", "MR_TAB.SPR", "MUNITS.SPR",
            "RBHAbndn.spr", "RBHAggro.spr", "RBHattk.spr", "RBHBsrk.spr", "RBHbuild.spr", "RBHChkn.spr", "RBHcncl.spr", "RBHcnclM.spr",
            "RBHCtrlB.spr", "RBHFrze.spr", "RBHgo.spr", "RBHInfl.spr", "RBHinfo.spr", "RBHPulse.spr", "RBHrsrch.spr", "RBHRtlt.spr",
            "RBHrtrt.spr", "RBHrturn.spr", "RBHScare.spr", "RBHstop.spr", "RBHsusp.spr", "RBHtnnl.spr", "RBHvwmap.spr", "RBHXplsv.spr",
            "RBMAbndn.spr", "RBMAggro.spr", "RBMATTK.SPR", "RBMBDUST.SPR", "RBMBsrk.spr", "RBMBUILD.SPR", "RBMChkn.spr", "RBMCNCL.SPR",
            "RBMcnclM.spr", "RBMCtrlB.spr", "RBMFrze.spr", "RBMgo.spr", "RBMHALT.SPR", "RBMinflt.spr", "RBMINFO.SPR", "RBMPulse.spr",
            "RBMRtlt.spr", "RBMRTRT.SPR", "RBMRTURN.SPR", "RBMSBOMB.SPR", "RBMscan.spr", "RBMScare.spr", "RBMstop.spr", "RBMsusp.spr",
            "RBMVWMAP.SPR", "RES-BUT.SPR", "rese-but.spr", "RP_Bio.spr", "RP_Blcks.spr", "RP_bm.spr", "RP_Cnstr.spr", "RP_Comms.spr",
            "RP_cons.spr", "RP_Coppr.spr", "RP_DM.spr", "RP_DR.spr", "RP_el.spr", "RP_EWP.spr", "RP_Farm.spr", "RP_FLM.spr", "RP_fm.spr",
            "RP_HElmn.spr", "RP_hm.spr", "RP_HRT.spr", "RP_HX.spr", "RP_mods.spr", "RP_Obs.spr", "RP_Power.spr", "RP_Proj.spr", "RP_Rapid.spr",
            "RP_Repr.spr", "RP_scn.spr", "RP_sm.spr", "RP_Susp.spr", "RP_T.spr", "RP_TC.spr", "RP_xt.spr", "UNIT-BUT.SPR",
        };

        public static bool IsEnhancedActive(Func<string, bool> exists) => exists(EnhancedMarker);
        public static bool IsLargerActive(Func<string, bool> exists) => exists(MapsRegular);

        private enum UnitVariant { None, Enhanced, Larger, Unknown }

        // which variant of a unit sprite is currently in DAT\, worked out from which store is missing its copy
        private static UnitVariant CurrentUnitVariant(string file, Func<string, bool> exists)
        {
            bool inDat = exists($"{Dat}\\{file}");
            bool inHd = exists($"{HdStore}\\{file}");
            bool inUnits = exists($"{UnitsStore}\\{file}");
            if (!inDat) { return UnitVariant.None; }
            if (!inUnits && inHd) { return UnitVariant.Larger; }
            if (!inHd && inUnits) { return UnitVariant.Enhanced; }
            return UnitVariant.Unknown; // both stores full (a loose file of unknown origin) or both empty
        }

        /// <summary>
        /// Returns the moves needed to reach the requested state. The plan is idempotent: files already
        /// in the right place produce no moves, so it can be used to repair a half swapped install.
        /// </summary>
        public static List<(string From, string To)> PlanMoves(bool enhanced, bool larger, Func<string, bool> exists)
        {
            var moves = new List<(string From, string To)>();
            foreach (string file in EnhancedFiles)
            {
                string dat = $"{Dat}\\{file}", store = $"{HdStore}\\{file}";
                if (enhanced && !exists(dat)) { moves.Add((store, dat)); }
                else if (!enhanced && exists(dat)) { moves.Add((dat, store)); }
            }
            UnitVariant wanted = larger ? UnitVariant.Larger : enhanced ? UnitVariant.Enhanced : UnitVariant.None;
            foreach (string file in UnitSprites)
            {
                string dat = $"{Dat}\\{file}";
                UnitVariant current = CurrentUnitVariant(file, exists);
                if (current == wanted) { continue; }
                // send the current variant back to its store first, then bring in the wanted one
                if (current == UnitVariant.Larger) { moves.Add((dat, $"{UnitsStore}\\{file}")); }
                else if (current == UnitVariant.Enhanced) { moves.Add((dat, $"{HdStore}\\{file}")); }
                else if (current == UnitVariant.Unknown) { moves.Add((dat, $"{UnitsStore}\\{file}")); } // Validate reports the clash
                if (wanted == UnitVariant.Larger) { moves.Add(($"{UnitsStore}\\{file}", dat)); }
                else if (wanted == UnitVariant.Enhanced) { moves.Add(($"{HdStore}\\{file}", dat)); }
            }
            bool mapsLarger = exists(MapsRegular);
            if (larger && !mapsLarger)
            {
                moves.Add((Maps, MapsRegular));
                moves.Add((MapsLarger, Maps));
            }
            else if (!larger && mapsLarger)
            {
                moves.Add((Maps, MapsLarger));
                moves.Add((MapsRegular, Maps));
            }
            return moves;
        }
    }
}
