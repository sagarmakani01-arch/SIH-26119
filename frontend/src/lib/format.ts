export function formatNumber(value: number, digits = 2): string {
  if (!Number.isFinite(value)) return "—";
  return new Intl.NumberFormat("en-IN", {
    maximumFractionDigits: digits,
    minimumFractionDigits: 0,
  }).format(value);
}

export function formatCompact(value: number): string {
  if (!Number.isFinite(value)) return "—";
  const abs = Math.abs(value);
  if (abs >= 1e7) return `${(value / 1e7).toFixed(2)} Cr`;
  if (abs >= 1e5) return `${(value / 1e5).toFixed(2)} L`;
  if (abs >= 1e3) return `${(value / 1e3).toFixed(1)} K`;
  return formatNumber(value, 2);
}

export function formatCurrencyINR(value: number, currency = "₹"): string {
  if (!Number.isFinite(value)) return "—";
  return `${currency}${formatCompact(value)}`;
}

export function formatPercent(fraction: number, digits = 1): string {
  if (!Number.isFinite(fraction)) return "—";
  return `${(fraction * 100).toFixed(digits)}%`;
}

export function formatSeconds(seconds: number): string {
  if (!Number.isFinite(seconds)) return "—";
  if (seconds < 0.001) return `${(seconds * 1e6).toFixed(0)} µs`;
  if (seconds < 1) return `${(seconds * 1000).toFixed(1)} ms`;
  return `${seconds.toFixed(2)} s`;
}

export function formatMs(ms: number): string {
  if (ms < 1000) return `${Math.round(ms)} ms`;
  return `${(ms / 1000).toFixed(2)} s`;
}

export function statusTone(
  status: string,
): "good" | "warn" | "bad" | "neutral" {
  switch (status) {
    case "OPTIMAL":
      return "good";
    case "FEASIBLE":
      return "warn";
    case "INFEASIBLE":
    case "UNBOUNDED":
    case "NUMERICAL_ERROR":
    case "ERROR":
      return "bad";
    case "TIME_LIMIT":
    case "ITERATION_LIMIT":
      return "warn";
    default:
      return "neutral";
  }
}
