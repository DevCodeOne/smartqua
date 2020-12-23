import { Component } from 'preact';
import { Router } from 'preact-router';
import { Link } from 'preact-router/match';

import { Home } from './home.js';
import { Settings } from './settings.js';
import { ScaleContextProvider } from './scalecontext.js'

import 'materialize-css/dist/js/materialize.min.js'
import 'materialize-css/dist/css/materialize.min.css'

export default class App extends Component {

	render() {
		return (
            <div>
                <nav>
                    <div>
                        <ul id="nav-mobile" class="right">
                            <li><Link activeClassName="active" href="/">Home</Link></li>
                            <li><Link activeClassName="active" href="/settings">Settings</Link></li>
                        </ul>
                    </div>
                </nav>
                <ScaleContextProvider>
                <Router>
                        <Home path="/" />
                        <Settings path="/settings" />
			    </Router>
                </ScaleContextProvider>
            </div>
		);
	}
}
