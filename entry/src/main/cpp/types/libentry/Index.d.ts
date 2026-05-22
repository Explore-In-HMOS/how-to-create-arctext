/**
 * Options for drawing arced text.
 *
 * `fontSize` and `radius` are optional. When omitted, the native side picks
 * defaults from the actual canvas size so the same call looks right on any
 * circular face (466, 454, 390, ...).
 */
export interface ArcTextOptions {
  /** The NodeContent slot from `<ContentSlot>`. */
  content: unknown;
  /** UTF-8 string to render. */
  text: string;
  /** Font size in px. Omit for an auto size that scales with the screen. */
  fontSize?: number;
  /** Baseline radius in px from screen center. Omit for an auto radius near the edge. */
  radius?: number;
  /** ARGB color, e.g. 0xFFFFFFFF (default white). */
  color?: number;
  /** Angle of text center; 0 = top, positive = clockwise (default 0). */
  startAngleDeg?: number;
  /** If true, glyphs face inward (use for bottom arcs). */
  flipped?: boolean;
}

/**
 * Draw arced text into the given NodeContent slot.
 */
export const drawArcText: (options: ArcTextOptions) => void;
