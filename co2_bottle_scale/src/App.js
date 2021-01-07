import { BrowserRouter as Router, Switch, Route } from "react-router-dom"
import { createMuiTheme, CssBaseline, ThemeProvider } from '@material-ui/core';
import { Home } from './Home.js'
import { Settings } from './Settings.js'
import { Navigation } from './Navigation.js'

const theme = createMuiTheme({
  palette: {
    type: 'dark'
  }
});

function App() {
  return (
    <div className="App">
      <ThemeProvider theme={theme}>
        <CssBaseline />
        <Router>
          <Switch>
            <Route path="/Settings">
              <Settings />
            </Route>
            <Route path="/">
              <Home />
            </Route>
          </Switch>
          <Navigation />
        </Router>
      </ThemeProvider>
    </div>
  );
}

export default App;
