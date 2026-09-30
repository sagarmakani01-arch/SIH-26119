const stages = [
  { label: "REAL-WORLD DATA", sub: "Inputs" },
  { label: "MATHEMATICAL MODEL", sub: "LP · MILP" },
  { label: "OPTIMIZATION ENGINE", sub: "C++ core" },
  { label: "CPU / GPU", sub: "Compute" },
  { label: "OPTIMAL SOLUTION", sub: "Verified" },
];

export function PipelineViz() {
  return (
    <div className="panel relative overflow-hidden px-5 py-6 sm:px-8 sm:py-7">
      <div className="pointer-events-none absolute inset-0 bg-[radial-gradient(600px_200px_at_50%_0%,rgba(34,211,238,0.07),transparent_70%)]" />
      <div className="relative flex flex-col items-stretch gap-3 sm:flex-row sm:items-center sm:justify-between">
        {stages.map((stage, i) => (
          <div key={stage.label} className="flex flex-1 items-center">
            <div className="animate-float w-full rounded-xl border border-line-2 bg-panel-2/80 px-3 py-3 text-center">
              <div className="num text-[10px] font-bold tracking-[0.12em] text-accent">
                0{i + 1}
              </div>
              <div className="mt-1 text-[11px] font-bold tracking-[0.1em] text-ink">
                {stage.label}
              </div>
              <div className="mt-0.5 text-[10px] text-ink-3">{stage.sub}</div>
            </div>
            {i < stages.length - 1 ? (
              <svg
                className="mx-1 hidden h-6 w-6 shrink-0 text-accent sm:block"
                viewBox="0 0 24 24"
                fill="none"
              >
                <path
                  d="M4 12h16M14 6l6 6-6 6"
                  stroke="currentColor"
                  strokeWidth="1.5"
                  className="animate-dash"
                />
              </svg>
            ) : null}
          </div>
        ))}
      </div>
      <div className="relative mt-5 flex flex-wrap items-center justify-center gap-2 text-[11px] text-ink-3">
        <span className="rounded-md border border-line-2 bg-panel-2 px-2 py-0.5">Deterministic</span>
        <span className="rounded-md border border-line-2 bg-panel-2 px-2 py-0.5">Verified primal/dual</span>
        <span className="rounded-md border border-line-2 bg-panel-2 px-2 py-0.5">Auditable JSON results</span>
        <span className="rounded-md border border-line-2 bg-panel-2 px-2 py-0.5">No fabricated metrics</span>
      </div>
    </div>
  );
}
