import './style';
import { Component } from 'preact';
import { Router } from 'preact-router';
import { Link } from 'preact-router/match';

import { Home } from './home.js';
import { Settings } from './settings.js';
import { ScaleContextProvider } from './scalecontext.js'

const initialData = { apiAdress : "SomeApiAddress" };

export default class App extends Component {

	render() {
		return (
            <div>
                <ScaleContextProvider>
                <nav>
                    <Link activeClassName="active" href="/">Home</Link>
                    <Link activeClassName="active" href="/settings">Settings</Link>
                </nav>
                <Router>
                        <Home path="/" />
                        <Settings path="/settings" />
			    </Router>
                </ScaleContextProvider>
            </div>
		);
	}
}
