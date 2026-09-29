import { useEffect, useId, useLayoutEffect, useRef, useState } from "react";
import { createPortal } from "react-dom";
import { useGameAsset } from "../game-assets";
import { formatModText } from "../mod-text";
import { GameIcon } from "./pc-game-icon";

/** A material choice is separate from the button that executes the craft. */
export function CraftChoice({name, value, label, assetKey, selected, blocked = false, itemClass, onChoose}: {
    name: string; value: string; label: string; assetKey: string; selected: boolean;
    blocked?: boolean; itemClass?: string; onChoose: () => void;
}) {
    const asset = useGameAsset(assetKey);
    const id = useId();
    const button = useRef<HTMLButtonElement>(null);
    const tooltip = useRef<HTMLDivElement>(null);
    const [open, setOpen] = useState(false);
    const [position, setPosition] = useState({left: 0, top: 0});
    const modifier = itemClass && asset?.essence_mods?.[itemClass];
    const details = Boolean(asset?.description || asset?.essence_mods);
    useLayoutEffect(() => {
        if (!open || !button.current || !tooltip.current) return;
        const reposition = () => {
            if (!button.current || !tooltip.current) return;
            const anchor = button.current.getBoundingClientRect();
            const box = tooltip.current.getBoundingClientRect();
            const below = anchor.bottom + 6;
            setPosition({
                left: Math.max(8, Math.min(anchor.left, window.innerWidth - box.width - 8)),
                top: Math.max(8, below + box.height <= window.innerHeight - 8 ? below : anchor.top - box.height - 6),
            });
        };
        const scroll = () => {
            const anchor = button.current?.getBoundingClientRect();
            if (!anchor || !button.current?.contains(document.elementFromPoint(
                anchor.left + anchor.width / 2, anchor.top + anchor.height / 2,
            ))) { setOpen(false); return; }
            // Focusing a selected tier can scroll its pane after pointer entry.
            // Keep the visible choice's tooltip attached through that scroll.
            reposition();
        };
        reposition();
        window.addEventListener("scroll", scroll, true);
        return () => window.removeEventListener("scroll", scroll, true);
    }, [open, asset, itemClass]);
    useEffect(() => {
        if (!open) return;
        const close = () => setOpen(false);
        const key = (event: KeyboardEvent) => { if (event.key === "Escape") close(); };
        window.addEventListener("resize", close);
        window.addEventListener("keydown", key);
        return () => {
            window.removeEventListener("resize", close);
            window.removeEventListener("keydown", key);
        };
    }, [open]);
    return <>
        <button ref={button} type="button" className="pc-material-choice" data-mechanic={name} data-value={value}
            aria-pressed={selected} aria-disabled={blocked} aria-describedby={open && details ? id : undefined}
            onPointerEnter={() => setOpen(true)} onPointerLeave={() => setOpen(false)}
            onFocus={() => setOpen(true)} onBlur={() => setOpen(false)}
            onClick={() => { if (!blocked) onChoose(); }}>
            <GameIcon assetKey={assetKey} /><span>{label}</span>
        </button>
        {open && details && createPortal(<div ref={tooltip} id={id} role="tooltip" className="pc-material-tooltip" style={position}>
            <strong>{asset?.name ?? label}</strong>
            {asset?.description && <div>{formatModText(asset.description)}</div>}
            {asset?.essence_mods && itemClass && <div className="pc-material-guarantee">
                <span>{itemClass}</span>
                {modifier ? formatModText(modifier) : "No modifier listed for this item class."}
            </div>}
        </div>, document.body)}
    </>;
}
