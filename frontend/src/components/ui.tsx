import type { ButtonHTMLAttributes, InputHTMLAttributes, ReactNode, SelectHTMLAttributes } from "react";
import { Loader2 } from "lucide-react";

type Tone = "accent" | "good" | "warn" | "bad" | "neutral" | "violet";

export function Button({
  variant = "primary",
  className = "",
  children,
  ...rest
}: ButtonHTMLAttributes<HTMLButtonElement> & {
  variant?: "primary" | "secondary" | "ghost" | "danger";
}) {
  const base =
    "focus-ring inline-flex items-center justify-center gap-2 rounded-xl px-5 py-2.5 text-sm font-semibold transition-all duration-150 disabled:cursor-not-allowed disabled:opacity-45";
  const variants: Record<string, string> = {
    primary:
      "bg-accent text-[#04222b] hover:bg-[#5ee3f7] shadow-[0_6px_24px_rgba(34,211,238,0.25)]",
    secondary: "bg-panel-2 border border-line-2 text-ink hover:border-accent/50 hover:text-accent",
    ghost: "text-ink-2 hover:text-ink hover:bg-panel-2/70",
    danger: "bg-panel-2 border border-bad/40 text-bad hover:bg-bad/10",
  };
  return (
    <button className={`${base} ${variants[variant]} ${className}`} {...rest}>
      {children}
    </button>
  );
}

export function Badge({ tone = "neutral", children }: { tone?: Tone; children: ReactNode }) {
  const tones: Record<Tone, string> = {
    accent: "bg-accent/12 text-accent border-accent/35",
    good: "bg-good/12 text-good border-good/35",
    warn: "bg-warn/12 text-warn border-warn/35",
    bad: "bg-bad/12 text-bad border-bad/35",
    violet: "bg-accent-2/15 text-[#c4b5fd] border-accent-2/40",
    neutral: "bg-panel-2 text-ink-2 border-line-2",
  };
  return (
    <span
      className={`inline-flex items-center gap-1.5 rounded-full border px-2.5 py-0.5 text-[11px] font-semibold uppercase tracking-wider ${tones[tone]}`}
    >
      {children}
    </span>
  );
}

export function Panel({
  children,
  className = "",
}: {
  children: ReactNode;
  className?: string;
}) {
  return <div className={`panel ${className}`}>{children}</div>;
}

export function Stat({
  label,
  value,
  unit,
  tone = "neutral",
}: {
  label: string;
  value: ReactNode;
  unit?: string;
  tone?: Tone;
}) {
  const color =
    tone === "good"
      ? "text-good"
      : tone === "warn"
        ? "text-warn"
        : tone === "bad"
          ? "text-bad"
          : tone === "accent"
            ? "text-accent"
            : tone === "violet"
              ? "text-[#c4b5fd]"
              : "text-ink";
  return (
    <div className="panel-soft px-4 py-3">
      <div className="label-cap">{label}</div>
      <div className={`num mt-1 text-xl font-semibold ${color}`}>
        {value}
        {unit ? <span className="ml-1 text-sm font-normal text-ink-3">{unit}</span> : null}
      </div>
    </div>
  );
}

export function Spinner({ size = 16 }: { size?: number }) {
  return <Loader2 size={size} className="animate-spin" />;
}

export function Field({
  label,
  unit,
  hint,
  error,
  children,
}: {
  label: string;
  unit?: string;
  hint?: string;
  error?: string;
  children: ReactNode;
}) {
  return (
    <label className="block">
      <div className="flex items-baseline justify-between gap-2">
        <span className="text-[13px] font-medium text-ink-2">{label}</span>
        {unit ? <span className="num text-[11px] text-ink-3">{unit}</span> : null}
      </div>
      <div className="mt-1.5">{children}</div>
      {hint ? <div className="mt-1 text-[11px] text-ink-3">{hint}</div> : null}
      {error ? <div className="mt-1 text-[11px] text-bad">{error}</div> : null}
    </label>
  );
}

const inputClass =
  "focus-ring num w-full rounded-lg border border-line-2 bg-bg-2/80 px-3 py-2 text-sm text-ink placeholder:text-ink-3 transition-colors hover:border-line-2/80 focus:border-accent/60";

export function NumberInput(props: InputHTMLAttributes<HTMLInputElement>) {
  return <input type="number" className={inputClass} {...props} />;
}

export function TextInput(props: InputHTMLAttributes<HTMLInputElement>) {
  return <input type="text" className={inputClass} {...props} />;
}

export function SelectInput(props: SelectHTMLAttributes<HTMLSelectElement>) {
  return <select className={inputClass} {...props} />;
}

export function Banner({
  tone,
  title,
  children,
}: {
  tone: "good" | "warn" | "bad" | "info";
  title: string;
  children?: ReactNode;
}) {
  const tones = {
    good: "border-good/40 bg-good/8 text-good",
    warn: "border-warn/40 bg-warn/8 text-warn",
    bad: "border-bad/40 bg-bad/8 text-bad",
    info: "border-accent/35 bg-accent/8 text-accent",
  } as const;
  return (
    <div className={`rounded-xl border px-4 py-3 text-sm ${tones[tone]}`}>
      <div className="font-semibold">{title}</div>
      {children ? <div className="mt-1 text-ink-2">{children}</div> : null}
    </div>
  );
}

export function ProgressBar({ indeterminate = false }: { indeterminate?: boolean }) {
  if (indeterminate) {
    return (
      <div className="h-2 w-full overflow-hidden rounded-full bg-panel-2">
        <div className="h-full w-1/3 animate-[pulse-soft_1.4s_ease-in-out_infinite] rounded-full bg-gradient-to-r from-accent to-accent-2" />
      </div>
    );
  }
  return (
    <div className="h-2 w-full overflow-hidden rounded-full bg-panel-2">
      <div className="h-full w-2/3 rounded-full bg-gradient-to-r from-accent to-accent-2" />
    </div>
  );
}

export function PageHeader({
  eyebrow,
  title,
  description,
  right,
}: {
  eyebrow: string;
  title: string;
  description?: string;
  right?: ReactNode;
}) {
  return (
    <div className="mb-6 flex flex-wrap items-end justify-between gap-4">
      <div>
        <div className="label-cap text-accent">{eyebrow}</div>
        <h1 className="mt-1 text-2xl font-bold tracking-tight text-ink">{title}</h1>
        {description ? <p className="mt-1 max-w-2xl text-sm text-ink-2">{description}</p> : null}
      </div>
      {right}
    </div>
  );
}

export function EmptyState({
  icon,
  title,
  body,
  action,
}: {
  icon: ReactNode;
  title: string;
  body: string;
  action?: ReactNode;
}) {
  return (
    <div className="panel flex flex-col items-center px-6 py-14 text-center">
      <div className="mb-4 grid h-14 w-14 place-items-center rounded-2xl border border-line-2 bg-panel-2 text-ink-3">
        {icon}
      </div>
      <h3 className="text-lg font-semibold text-ink">{title}</h3>
      <p className="mt-1.5 max-w-md text-sm text-ink-2">{body}</p>
      {action ? <div className="mt-5">{action}</div> : null}
    </div>
  );
}
