/** Strip presentation markup only. Numeric roll ranges and mechanical qualifiers
 * remain intact; this formatter never interprets or changes crafting rules. */
export function formatModText(text: string): string {
    return text
        .replace(/\[([^\[\]|]+)\|([^\[\]]+)\]/g, "$2")
        .replace(/\[([^\[\]|]+)\]/g, "$1")
        .replace(/<\w+>\{([^{}]*)\}/g, "$1")
        .replace(/\(([+-]?\d+(?:\.\d+)?)[-–]([+-]?\d+(?:\.\d+)?)\)/g, "$1–$2")
        .replace(/\\n/g, "\n")
        .replace(/\r\n/g, "\n")
        .trim();
}

export function modTextLines(lines: readonly string[]): string[] {
    return lines.flatMap(line => formatModText(line).split("\n")).filter(Boolean);
}

export function modTextLabel(lines: readonly string[], fallback = ""): string {
    return modTextLines(lines).join(" / ") || formatModText(fallback);
}
