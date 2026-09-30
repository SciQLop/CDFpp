// Mouse, trackpad and touch gestures for one uPlot chart, the same as speasy-proxy's /plot
// (speasy_proxy/static/js/plot-gestures.js, without its event marking). Plot area: wheel /
// pinch = zoom x at the cursor, horizontal swipe / Shift+wheel / drag = pan x, double-click
// = whole variable (uPlot's own reset); on touch, one finger pans and two fingers pinch-zoom.
// Y-axis gutter: wheel zooms and drag pans Y, double-click resets Y.
// The math is pure and unit-tested in Node (tests/wacdfpp_plot).

const WHEEL_LINE_PX = 16;   // a "line" of wheel delta ≈ 16px
const WHEEL_PAGE_PX = 800;  // a "page" of wheel delta ≈ 800px
const WHEEL_MAX_PX = 120;   // clamp so one big notch can't overshoot
const ZOOM_SENSITIVITY = 0.0015;  // zoom amount per normalized wheel pixel
// simplify: tuned by reasoning in speasy-proxy, not on hardware; pinch deltas are ~10x
// smaller than wheel notches. Raise/lower if pinch feels sluggish/jumpy on a real trackpad.
const PINCH_ZOOM_SENSITIVITY = 0.01;

// A wheel event's delta in pixels whatever the device/deltaMode, clamped to ±WHEEL_MAX_PX.
export function normalizeWheelDelta(delta, deltaMode) {
    let px = delta;
    if (deltaMode === 1) px = delta * WHEEL_LINE_PX;
    else if (deltaMode === 2) px = delta * WHEEL_PAGE_PX;
    return Math.max(-WHEEL_MAX_PX, Math.min(WHEEL_MAX_PX, px));
}

// What a wheel event means: 'pinch' (trackpad pinch arrives as Ctrl+wheel), 'pan'
// (Shift+wheel or a mostly-horizontal swipe) or 'zoom'. px is the normalized delta.
// Shift reads X too because Chrome already swaps Shift+wheel onto deltaX.
export function wheelIntent({ deltaX, deltaY, deltaMode, shiftKey, ctrlKey }) {
    const px = (d) => normalizeWheelDelta(d, deltaMode);
    if (ctrlKey) return { kind: "pinch", px: px(deltaY) };
    if (shiftKey) return { kind: "pan", px: px(deltaY || deltaX) };
    if (Math.abs(deltaX) > Math.abs(deltaY)) return { kind: "pan", px: px(deltaX) };
    return { kind: "zoom", px: px(deltaY) };
}

// Zoom [start, end] around cursorFrac (0..1 across it): factor < 0 zooms in, > 0 out; the
// value under the cursor stays put. Null when the result would be narrower than minSpan.
export function zoomToward(start, end, cursorFrac, factor, minSpan) {
    const center = start + cursorFrac * (end - start);
    const next = { start: center - (center - start) * (1 + factor), end: center + (end - center) * (1 + factor) };
    return next.end - next.start < minSpan ? null : next;
}

// Shift [start, end] by a fraction of its width (positive = later).
export function panRange(start, end, fraction) {
    const shift = (end - start) * fraction;
    return { start: start + shift, end: end + shift };
}

// Two-finger pinch: the values under the fingers at touch-down ([f1, f2], fractions of the
// width across view0) stay under the fingers at [g1, g2]. One formula covers zoom and pan.
// Null when the fingers meet or cross, or below minSpan.
export function pinchRange(view0, [f1, f2], [g1, g2], minSpan) {
    const span0 = view0.end - view0.start;
    const t1 = view0.start + f1 * span0;
    const t2 = view0.start + f2 * span0;
    const span = (t2 - t1) / (g2 - g1);
    if (!Number.isFinite(span) || span < minSpan) return null;
    const start = t1 - g1 * span;
    return { start, end: start + span };
}

// Y range shown between two pixel rows (0 = top of the plot area), through a snapshot of the
// scale ({ min, max, log, heightPx }): pixels make pan/zoom the same on linear and log axes.
// Null when degenerate.
export function yRangeFromPixels(scale, bottomPx, topPx) {
    const f = scale.log ? Math.log10 : (v) => v;
    const inv = scale.log ? (v) => 10 ** v : (v) => v;
    const lo = f(scale.min), hi = f(scale.max);
    const at = (px) => inv(lo + ((scale.heightPx - px) / scale.heightPx) * (hi - lo));
    const min = at(bottomPx), max = at(topPx);
    return Number.isFinite(min) && Number.isFinite(max) && max > min ? { min, max } : null;
}

// ctx: { minSpan, getView() -> { start, end }, setView({ start, end }), setY(min, max), resetY() }
export function bindGestures(u, ctx) {
    // The Y gutter is the strip left of the plot area, at the plot area's height.
    const inGutter = (e) => {
        const r = u.over.getBoundingClientRect();
        return e.clientX < r.left && e.clientY >= r.top && e.clientY <= r.bottom;
    };

    // Wheel anywhere else is left alone so the page can scroll.
    u.root.addEventListener("wheel", (e) => {
        const onY = inGutter(e);
        if (!onY && !u.over.contains(e.target)) return;
        e.preventDefault();
        const intent = wheelIntent(e);
        if (onY) wheelY(u, ctx, e, intent);
        else wheelX(u, ctx, e, intent);
    }, { passive: false });

    u.root.addEventListener("mousedown", (e) => {
        if (e.button === 0 && inGutter(e)) dragY(u, ctx, e);
    });
    u.over.style.touchAction = "none";
    bindXDrag(u, ctx);

    u.root.addEventListener("dblclick", (e) => {
        if (!inGutter(e)) return;
        e.preventDefault();
        ctx.resetY();
    });
}

const zoomFactor = ({ kind, px }) => px * (kind === "pinch" ? PINCH_ZOOM_SENSITIVITY : ZOOM_SENSITIVITY);

// Pans move the content by the swipe's pixels, so it tracks the fingers like a drag.
function wheelX(u, ctx, e, intent) {
    const { start, end } = ctx.getView();
    const rect = u.over.getBoundingClientRect();
    if (intent.kind === "pan") {
        ctx.setView(panRange(start, end, intent.px / (rect.width || 1)));
        return;
    }
    const frac = Math.max(0, Math.min(1, (e.clientX - rect.left) / rect.width));
    const next = zoomToward(start, end, frac, zoomFactor(intent), ctx.minSpan);
    if (next) ctx.setView(next);
}

function yScaleSnapshot(u) {
    const sc = u.scales.y;
    return { min: sc.min, max: sc.max, log: sc.distr === 3, heightPx: u.over.clientHeight || 1 };
}

function applyY(ctx, range) {
    if (range) ctx.setY(range.min, range.max);
}

function wheelY(u, ctx, e, intent) {
    const scale = yScaleSnapshot(u);
    const h = scale.heightPx;
    if (intent.kind === "pan") {
        applyY(ctx, yRangeFromPixels(scale, h + intent.px, intent.px));
        return;
    }
    const cursorPx = e.clientY - u.over.getBoundingClientRect().top;
    const f = 1 + zoomFactor(intent);
    applyY(ctx, yRangeFromPixels(scale, cursorPx + (h - cursorPx) * f, cursorPx - cursorPx * f));
}

// Pointers down on the plot area, by id, in touch order. The gesture is re-anchored to the
// current view whenever a finger lands or lifts, so two fingers to one continues as a pan.
function bindXDrag(u, ctx) {
    const pointers = new Map();  // pointerId -> clientX
    let anchor = null;           // { view, fracs } at the last change of finger count
    const fracs = () => {
        const r = u.over.getBoundingClientRect();
        return [...pointers.values()].slice(0, 2).map((x) => (x - r.left) / (r.width || 1));
    };
    const reanchor = () => { anchor = { view: ctx.getView(), fracs: fracs() }; };

    u.over.addEventListener("pointerdown", (e) => {
        if (e.pointerType === "mouse" && e.button !== 0) return;
        u.over.setPointerCapture(e.pointerId);
        pointers.set(e.pointerId, e.clientX);
        document.body.style.userSelect = "none";
        reanchor();
    });
    u.over.addEventListener("pointermove", (e) => {
        if (!pointers.has(e.pointerId)) return;
        pointers.set(e.pointerId, e.clientX);
        const now = fracs();
        const { view, fracs: then } = anchor;
        const next = now.length === 2
            ? pinchRange(view, then, now, ctx.minSpan)
            : panRange(view.start, view.end, then[0] - now[0]);
        if (next) ctx.setView(next);
    });
    const lift = (e) => {
        if (!pointers.delete(e.pointerId)) return;
        if (pointers.size === 0) document.body.style.userSelect = "";
        reanchor();
    };
    u.over.addEventListener("pointerup", lift);
    u.over.addEventListener("pointercancel", lift);
}

// Anchored to the scale at mousedown: the drag keeps changing the live scale, so reading it
// on every move would compound the shift.
function dragY(u, ctx, e) {
    e.preventDefault();
    const scale = yScaleSnapshot(u);
    const y0 = e.clientY;
    document.body.style.userSelect = "none";
    const move = (m) => {
        const dy = m.clientY - y0;
        applyY(ctx, yRangeFromPixels(scale, scale.heightPx - dy, -dy));
    };
    const up = () => {
        document.body.style.userSelect = "";
        window.removeEventListener("mousemove", move);
        window.removeEventListener("mouseup", up);
    };
    window.addEventListener("mousemove", move);
    window.addEventListener("mouseup", up);
}
