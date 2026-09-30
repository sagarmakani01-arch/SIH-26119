import { NavLink, Link, useLocation } from "react-router-dom";
import { Cpu, Gauge, Home, Layers, LayoutGrid, BookOpen, Play, Workflow } from "lucide-react";
import { useBackend } from "../services/BackendContext";
import type { ReactNode } from "react";

const links = [
  { to: "/", label: "Home", icon: Home, end: true },
  { to: "/optimizer", label: "Optimizer", icon: Gauge },
  { to: "/models", label: "Models", icon: Layers },
  { to: "/scenarios", label: "Scenarios", icon: Workflow },
  { to: "/benchmarks", label: "Benchmarks", icon: LayoutGrid },
  { to: "/documentation", label: "Documentation", icon: BookOpen },
];

function StatusDot() {
  const { state } = useBackend();
  if (state.kind === "loading") {
    return (
      <span className="flex items-center gap-2 text-xs text-ink-3">
        <span className="h-2 w-2 rounded-full bg-ink-3 animate-pulse-soft" />
        Checking…
      </span>
    );
  }
  if (state.kind === "down") {
    return (
      <Link
        to="/status"
        className="flex items-center gap-2 rounded-full border border-bad/40 bg-bad/10 px-3 py-1 text-xs font-semibold text-bad transition-colors hover:bg-bad/20"
        title={state.message}
      >
        <span className="h-2 w-2 rounded-full bg-bad" />
        API Offline
      </Link>
    );
  }
  const solverOk = state.health.solver_cli.available;
  return (
    <Link
      to="/status"
      className="flex items-center gap-2 rounded-full border border-good/40 bg-good/10 px-3 py-1 text-xs font-semibold text-good transition-colors hover:bg-good/20"
      title={`API ${state.health.status} · solver_cli ${solverOk ? "available" : "missing"}`}
    >
      <span className="h-2 w-2 rounded-full bg-good" />
      System Operational
    </Link>
  );
}

export function Nav() {
  const location = useLocation();
  return (
    <header className="sticky top-0 z-40 border-b border-line/80 bg-bg/85 backdrop-blur-xl">
      <div className="mx-auto flex h-16 max-w-7xl items-center gap-6 px-4 sm:px-6">
        <Link to="/" className="flex items-center gap-2.5 shrink-0">
          <span className="grid h-9 w-9 place-items-center rounded-xl bg-gradient-to-br from-accent to-accent-2 text-[#042f2e]">
            <Cpu size={18} strokeWidth={2.5} />
          </span>
          <span className="hidden leading-tight sm:block">
            <span className="block text-sm font-extrabold tracking-[0.18em] text-ink">
              SOVEREIGN
            </span>
            <span className="block text-[10px] font-medium tracking-[0.24em] text-ink-3">
              OPTIMIZATION ENGINE
            </span>
          </span>
        </Link>

        <nav className="flex flex-1 items-center gap-1 overflow-x-auto">
          {links.map(({ to, label, icon: Icon, end }) => {
            const active =
              end ? location.pathname === to : location.pathname.startsWith(to) && to !== "/";
            return (
              <NavLink
                key={to}
                to={to}
                end={end}
                className={`flex items-center gap-1.5 rounded-lg px-3 py-2 text-[13px] font-medium transition-colors ${
                  active
                    ? "bg-accent/12 text-accent"
                    : "text-ink-2 hover:bg-panel-2/80 hover:text-ink"
                }`}
              >
                <Icon size={15} />
                {label}
              </NavLink>
            );
          })}
        </nav>

        <div className="flex items-center gap-3 shrink-0">
          <StatusDot />
          <Link
            to="/optimizer"
            className="focus-ring hidden items-center gap-1.5 rounded-lg bg-accent px-3.5 py-2 text-[13px] font-bold text-[#042f2e] transition-colors hover:bg-[#5eead4] sm:inline-flex"
          >
            <Play size={14} fill="currentColor" />
            New Optimization
          </Link>
        </div>
      </div>
    </header>
  );
}

export function Layout({ children }: { children: ReactNode }) {
  return (
    <div className="flex min-h-screen flex-col">
      <Nav />
      <main className="mx-auto w-full max-w-7xl flex-1 px-4 py-8 sm:px-6">{children}</main>
      <footer className="border-t border-line/80 bg-bg-2/60">
        <div className="mx-auto flex max-w-7xl flex-wrap items-center justify-between gap-3 px-4 py-5 text-xs text-ink-3 sm:px-6">
          <span>
            Sovereign Optimization Engine — SIH 2026 PS 26119 · LP / MILP · CPU active, GPU-ready
          </span>
          <span className="flex items-center gap-4">
            <Link to="/status" className="hover:text-ink-2">
              System Status
            </Link>
            <Link to="/documentation" className="hover:text-ink-2">
              Documentation
            </Link>
            <Link to="/docs" className="hover:text-ink-2">
              API Swagger
            </Link>
          </span>
        </div>
      </footer>
    </div>
  );
}
