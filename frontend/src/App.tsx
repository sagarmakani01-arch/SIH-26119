import { BrowserRouter, Navigate, Route, Routes, useLocation } from "react-router-dom";
import { Layout } from "./components/Nav";
import { BackendProvider } from "./services/BackendContext";
import { WorkspaceProvider, useWorkspace } from "./workspace/WorkspaceContext";
import Landing from "./pages/Landing";
import Optimizer from "./pages/Optimizer";
import Guided from "./pages/Guided";
import Expert from "./pages/Expert";
import Review from "./pages/Review";
import Solving from "./pages/Solving";
import Results from "./pages/Results";
import Explain from "./pages/Explain";
import Scenarios from "./pages/Scenarios";
import Models from "./pages/Models";
import Benchmarks from "./pages/Benchmarks";
import Documentation from "./pages/Documentation";
import Status from "./pages/Status";
import type { ReactNode } from "react";

function RequireModel({ children }: { children: ReactNode }) {
  const { workspace } = useWorkspace();
  const location = useLocation();
  if (!workspace.model) {
    return <Navigate to="/optimizer" replace state={{ from: location.pathname }} />;
  }
  return <>{children}</>;
}

function RequireResult({ children }: { children: ReactNode }) {
  const { workspace } = useWorkspace();
  if (!workspace.model || !workspace.result) {
    return <Navigate to="/optimizer" replace />;
  }
  return <>{children}</>;
}

export default function App() {
  return (
    <BrowserRouter>
      <BackendProvider>
        <WorkspaceProvider>
          <Layout>
            <Routes>
              <Route path="/" element={<Landing />} />
              <Route path="/optimizer" element={<Optimizer />} />
              <Route path="/optimizer/guided/:templateId" element={<Guided />} />
              <Route path="/optimizer/expert" element={<Expert />} />
              <Route
                path="/optimizer/review"
                element={
                  <RequireModel>
                    <Review />
                  </RequireModel>
                }
              />
              <Route
                path="/optimizer/solving"
                element={
                  <RequireModel>
                    <Solving />
                  </RequireModel>
                }
              />
              <Route
                path="/optimizer/results"
                element={
                  <RequireModel>
                    <Results />
                  </RequireModel>
                }
              />
              <Route
                path="/optimizer/explain"
                element={
                  <RequireResult>
                    <Explain />
                  </RequireResult>
                }
              />
              <Route path="/scenarios" element={<Scenarios />} />
              <Route path="/models" element={<Models />} />
              <Route path="/benchmarks" element={<Benchmarks />} />
              <Route path="/documentation" element={<Documentation />} />
              <Route path="/status" element={<Status />} />
              <Route path="*" element={<Navigate to="/" replace />} />
            </Routes>
          </Layout>
        </WorkspaceProvider>
      </BackendProvider>
    </BrowserRouter>
  );
}
